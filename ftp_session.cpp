#include "ftp_session.hpp"
#include "yasio/obstream.hpp"
#include "ftp_server.hpp"
#include <iostream>

using namespace std; // for string literal

#if defined(_WIN32)
#  define localtime_r(tp, tr) localtime_s(tr, tp)
#endif

#define __service server_.service_
#define __root server_.root_

#ifdef _WIN32
#  define stat64 _stat64
#endif

namespace nzls
{
template <typename _CStr, typename _Fn>
inline void fast_split_of(_CStr s, size_t slen,
                          const typename std::remove_pointer<_CStr>::type* delims, _Fn func)
{
  auto _Start = s; // the start of every string
  auto _Ptr   = s; // source string iterator
  auto _End   = s + slen;
  auto _Delim = delims[0];
  while ((_Ptr = strpbrk(_Ptr, delims)))
  {
    if (_Start < _Ptr)
    {
      func(_Start, _Ptr, _Delim);
      _Delim = *_Ptr;
    }
    _Start = _Ptr + 1;
    ++_Ptr;
  }
  if (_Start < _End)
  {
    func(_Start, _End, _Delim);
  }
}
#if _HAS_CXX17
template <typename _Elem, typename _Fn>
inline void fast_split_of(std::basic_string_view<_Elem> s, const _Elem* delims, _Fn func)
{
  return fast_split_of(s.data(), s.length(), delims, func);
}
#endif
} // namespace nzls

static std::string to_string(ip::endpoint& ep, unsigned short port)
{
  std::stringstream ss;
  ss << "(" << (int)ep.in4_.sin_addr.s_net << "," << (int)ep.in4_.sin_addr.s_host << ","
     << (int)ep.in4_.sin_addr.s_lh << "," << (int)ep.in4_.sin_addr.s_impno << ","
     << ((port >> 8) & 0xff) << "," << (port & 0xff) << ")";

  return ss.str();
}
static void list_files(const std::string& dirPath,
                       const std::function<void(tinydir_file&)>& callback, bool recursively = false)
{
  if (fsutils::is_dir_exists(dirPath))
  {
    tinydir_dir dir;
    std::string fullpathstr = dirPath;

    if (tinydir_open(&dir, &fullpathstr[0]) != -1)
    {
      while (dir.has_next)
      {
        tinydir_file file;
        if (tinydir_readfile(&dir, &file) == -1)
        {
          // Error getting file
          break;
        }
        std::string fileName = file.name;

        if (fileName != "." && fileName != "..")
        {
          callback(file);
          if (file.is_dir && recursively)
            list_files(file.path, callback, recursively);
        }

        if (tinydir_next(&dir) == -1)
        {
          // Error getting next file
          break;
        }
      }
    }
    tinydir_close(&dir);
  }
}

////////////////////////// transmit_session ////////////////////////////
class transmit_session : public std::enable_shared_from_this<transmit_session>
{
public:
  static void start_transmit(std::string_view filename,
                             std::function<int(std::vector<char>, std::function<void()>)> send_cb,
                             std::function<void()> complete_cb)
  {
    auto session = std::make_shared<transmit_session>(filename, send_cb, complete_cb);
    session->start();
  }

public:
  transmit_session(std::string_view filename,
                   std::function<int(std::vector<char>, std::function<void()>)>& send_cb,
                   std::function<void()>& complete_cb)
      : send_cb_(std::move(send_cb)), complete_cb_(std::move(complete_cb))
  {
    this->fp_ = fopen(filename.data(), "rb");
  }
  ~transmit_session()
  {
    if (this->fp_)
      fclose(this->fp_);
    printf("the transmit_session: %p destroyed!\n", this);
  }

  void start()
  {
    if (this->fp_)
    {
      handle_write();
    }
  }

  void handle_write()
  {
    auto n = fread(buffer_, 1, YASIO_ARRAYSIZE(buffer_), this->fp_);
    if (n > 0)
    {
      auto self = shared_from_this();
      if (send_cb_(std::vector<char>(buffer_, buffer_ + n),
                   std::bind(&transmit_session::handle_write, self)) < 0)
      {
        complete_cb_();
      }
    }
    else
    { // done
      complete_cb_();
    }
  }

  std::function<int(std::vector<char>, std::function<void()>)> send_cb_;
  std::function<void()> complete_cb_;

  FILE* fp_ = nullptr;
  char buffer_[2 * 1024 * 1024];
};

////////////////////////// ftp_session /////////////////////////////////

ftp_session::ftp_session(ftp_server& server, transport_handle_t ctl)
    : server_(server), thandle_ctl_(ctl), thandle_transfer_(nullptr), status_(transfer_status::NONE)
{
  session_id_ = reinterpret_cast<int>(thandle_ctl_->ud_);
  path_       = "/";
}

// say hello to client, we can start ftp service
void ftp_session::say_hello()
{
  using namespace std; // for string literal operator 'sv'
  stock_reply("220"sv, u8"x-studio Pro embedded FTP Server © 2020."sv, false);
  stock_reply("220"sv, "Please visit https://x-studio.net/"sv);
}

void ftp_session::handle_packet(std::vector<char>& packet)
{
  std::string_view algsv(packet.data(), packet.size());
  size_t crlf;
  while ((crlf = algsv.find_last_of("\r\n")) != std::string_view::npos)
    algsv.remove_suffix(1);

  std::string cmd, param;
  auto offset = algsv.find_first_of(' ');
  if (offset != std::string_view::npos)
  {
    cmd    = algsv.substr(0, offset);
    offset = algsv.find_first_not_of(' ', offset + 1);
    if (offset != std::string_view::npos)
      param = algsv.substr(offset);
  }
  else
    cmd = algsv;

  cmd.resize(sizeof(ftp_cmd_id_t));

  std::cout << algsv << "\n";
  auto handler_id = *reinterpret_cast<const uint32_t*>(cmd.c_str());
  auto it         = handlers_.find(handler_id);
  if (it != handlers_.end())
    it->second(*this, param);
  else
  {
    using namespace std; // for string literal operator 'sv'
    stock_reply("500"sv, "Syntax Error."sv);
  }
}

void ftp_session::open_transimt_session(transport_handle_t thandle)
{
  this->thandle_transfer_ = thandle;
  do_transmit();
}

/// ---------- All supported commands handlers ------------
void ftp_session::process_USER(const std::string& param)
{
  stock_reply("331"sv, yasio::strfmt(127, "Password required for %s.", param.c_str()));
}
void ftp_session::process_PASS(const std::string& param)
{
  stock_reply("230"sv, "Login succeed."sv);
}
void ftp_session::process_SYST(const std::string& param) { stock_reply("215"sv, "WINDOWS"sv); }
void ftp_session::process_PWD(const std::string& param)
{
  stock_reply("215"sv, this->path_, true, true);
}
void ftp_session::process_TYPE(const std::string& param)
{
  stock_reply("200"sv, "Switching to binary mode."sv);
}
void ftp_session::process_SIZE(const std::string& param)
{
  auto size = fsutils::get_file_size(param);
  if (size > 0)
  {
    stock_reply("213"sv, std::to_string(size));
  }
  else
  {
    stock_reply("550"sv, fsutils::is_dir_exists(param) ? "not a plain file."sv
                                                       : "No such file or directory."sv);
  }
}
void ftp_session::process_CDUP(const std::string& /*param*/) { process_CWD(".."); }

static bool verify_path(std::string_view path, bool isdir)
{
  size_t pos = std::string::npos;
  int total  = isdir ? 0 : -1;
  int upcnt  = 0;
  nzls::fast_split_of(path, R"(/\)", [&](const char* s, const char* e, char) {
    ++total;
    if (e - s == 2 && *s == '.' && s[1] == '.')
      ++upcnt;
  });
  return upcnt <= (total / 2);
}
void ftp_session::process_CWD(const std::string& param)
{
  std::string path = path_;
  if (param == "..")
  {
    if (path.size() > 1)
    {
      auto offset = path.find_last_of('/');
      if (offset != std::string::npos)
      {
        if (offset == 0)
          ++offset;
        path.resize(offset);
      }
    }
  }
  else
  {
    if (*param.c_str() == '/')
      path = param;
    else
    {
      if (path.back() != '/')
        path.push_back('/');
      path.append(param);
    }
  }
  if (verify_path(path, true))
  {
    if (fsutils::is_dir_exists(__root + path))
    {
      this->path_ = path;
      stock_reply("250"sv, "OK."sv);
    }
    else
      stock_reply("550"sv,
                  yasio::strfmt(127, "CWD failed, \"%s\": directory not found.", param.c_str()));
  }
  else
    stock_reply("550"sv,
                yasio::strfmt(127, "CWD failed, \"%s\": directory invalid.", param.c_str()));
}
void ftp_session::process_PASV(const std::string& param)
{
  static unsigned short listening_port = 20525;
  ++listening_port;

  int cindex   = session_id_;
  auto channel = __service.cindex_to_handle(cindex);
  if (channel != nullptr)
  {
    if (!__service.is_open(cindex))
    {
      __service.set_option(YOPT_CHANNEL_LOCAL_PORT, cindex, listening_port);
      __service.open(cindex, YCM_TCP_SERVER);
    }

    std::string msg = "Entering passive mode ";
    msg += to_string(thandle_ctl_->local_endpoint(), channel->local_port());
    stock_reply("227", msg);
  }
  else
  { // not channle to transfer data
  }
}
void ftp_session::process_LIST(const std::string& param)
{
  stock_reply("150"sv, "Sending directory listing."sv);
  status_         = ftp_session::transfer_status::LIST;
  this->fullpath_ = __root + path_;
  this->do_transmit();
}
void ftp_session::process_RETR(const std::string& param)
{
  std::string fullpath;
  std::string path = param;
  if (*param.c_str() == '/')
    path = param;
  else
    path = path_ + "/" + param;

  if (verify_path(path, false) && fsutils::is_file_exists((fullpath = __root + path)))
  {
    stock_reply("150"sv, "Opening BINARY mode for file transfer."sv);
    status_         = ftp_session::transfer_status::FILE;
    this->fullpath_ = fullpath;
    this->do_transmit();
  }
  else
  {
    stock_reply("550"sv, "No such file or directory."sv);
  }
}
void ftp_session::process_QUIT(const std::string& param) { stock_reply("221"sv, "Bye."sv); }
void ftp_session::process_AUTH(const std::string& param)
{
  stock_reply("502"sv,
              yasio::strfmt(127, "Explicit %s authentication not allowed.", param.c_str()));
}
void ftp_session::process_OPTS(const std::string& param)
{
  stock_reply("211"sv, "Always in UTF8 mode."sv);
}
void ftp_session::process_FEAT(const std::string& param)
{
  stock_reply("211"sv, "Features:"sv, false);
  stock_reply(""sv, "UTF8"sv, false);
  stock_reply("211"sv, "End"sv);
}

void ftp_session::do_transmit()
{
  if (this->thandle_transfer_ != nullptr)
  {
    if (this->status_ == transfer_status::LIST)
    {
      printf("FTP-DATA: start transfer file list...\n");
      yasio::obstream obs;
      time_t tval = time(NULL);
      struct tm daytm;
      localtime_r(&tval, &daytm);

      list_files(this->fullpath_, [&](tinydir_file& f) {
        obs.write_bytes(f.is_dir ? "dr--r--r--"sv : "-r--r--r--");
        obs.write_bytes(f.is_dir ? " 2"sv : " 1"sv);
        struct stat64 st;
        if (0 == ::stat64(f.path, &st))
        {
          struct tm tinfo;
          localtime_r(&st.st_mtime, &tinfo);

          if (st.st_mode & S_IFREG)
          {
            obs.write_bytes(" "sv);
            obs.write_bytes(std::to_string(st.st_size));
          }
          else
          {
            obs.write_bytes(" 0"sv);
          }

          char buf[96];
          strftime(buf, 96, tinfo.tm_year == daytm.tm_year ? " %b  %e  %R" : " %b  %e  %Y", &tinfo);

          obs.write_bytes(buf);
        }

        obs.write_byte(' ');
        obs.write_bytes(f.name);
        obs.write_bytes("\n"sv);
      });

      if (!obs.empty())
        __service.write(this->thandle_transfer_, std::move(obs.buffer()),
                        [=]() { stock_reply("226"sv, "Done."sv); });
      else
        stock_reply("226"sv, "Done."sv);
    }
    else if (this->status_ == transfer_status::FILE)
    {
      printf("FTP-DATA: start transfer file: %s...\n", this->fullpath_.c_str());
      transmit_session::start_transmit(
          this->fullpath_,
          [=](std::vector<char> buffer, std::function<void()> handler) {
            return __service.write(this->thandle_transfer_, std::move(buffer), std::move(handler));
          },
          [=]() { stock_reply("226"sv, "Done."sv); });
    }
    else
    {
      printf("The connection established, wait command.\n");
    }
  }
  else
  {
    printf("Got client command ready, wait income connection.\n");
  }
}

void ftp_session::stock_reply(std::string_view code, std::string_view resp_data, bool finished,
                              bool ispath)
{
  printf("Reponse:%s, msg: %s\n", code.data(), resp_data.data());

  obstream obs;

  using namespace std; // for string literal operator 'sv'
  if (!code.empty())
  {

    obs.write_bytes(code);
    obs.write_bytes(finished ? " "sv : "-"sv);
    if (ispath)
      obs.write_byte('\"');
    obs.write_bytes(resp_data);
    if (ispath)
      obs.write_byte('\"');
    obs.write_bytes("\r\n"sv);
  }
  else
  {
    obs.write_bytes(resp_data);
    obs.write_bytes("\r\n"sv);
  }

  if (code == "226")
    this->status_ = transfer_status::NONE;

  __service.write(this->thandle_ctl_, std::move(obs.buffer()), [=]() {
    if (code == "226" && this->thandle_transfer_ != nullptr)
    {
      __service.close(this->thandle_transfer_);
      this->thandle_transfer_ = nullptr;
    }
  });
}

// register all supported commands' handler
void ftp_session::register_handlers_once()
{
#define XSFTPD_REGISTER(cmd) register_handler(#cmd, &process_##cmd);
  if (handlers_.empty())
  {
    XSFTPD_REGISTER(USER);
    XSFTPD_REGISTER(PASS);
    XSFTPD_REGISTER(SYST);
    XSFTPD_REGISTER(PWD);
    XSFTPD_REGISTER(TYPE);
    XSFTPD_REGISTER(SIZE);
    XSFTPD_REGISTER(CWD);
    XSFTPD_REGISTER(CDUP);
    XSFTPD_REGISTER(PASV);
    XSFTPD_REGISTER(LIST);
    XSFTPD_REGISTER(RETR);
    XSFTPD_REGISTER(QUIT);
    XSFTPD_REGISTER(AUTH);
    XSFTPD_REGISTER(OPTS);
    XSFTPD_REGISTER(FEAT);
  }
}

void ftp_session::register_handler(std::string cmd,
                                   std::function<void(ftp_session&, const std::string&)> handler)
{
  cmd.resize(sizeof(ftp_cmd_id_t));
  handlers_.emplace(*reinterpret_cast<const uint32_t*>(cmd.c_str()), std::move(handler));
}

std::unordered_map<ftp_cmd_id_t, std::function<void(ftp_session&, const std::string&)>>
    ftp_session::handlers_;
