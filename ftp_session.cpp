#include "ftp_session.hpp"
#include "yasio/obstream.hpp"
#include "ftp_server.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>

#include <tinydir/tinydir.h>

#if YASIO__HAS_CXX17
using namespace std;
#  define _mksv(a) a ""sv
#else
template <size_t size>
inline cxx17::string_view _mksv(const char (&strLiteral)[size])
{
  return cxx17::string_view(strLiteral, size - 1);
}
#  define u8
#endif

#define __service server_.service_
#define __wwwroot server_.root_
#define __wanip server_.wanip_

extern long long g_stats_hits;

////////////////////////// transmit_session ////////////////////////////
class transmit_session : public std::enable_shared_from_this<transmit_session> {
public:
  template <typename... _Types>
  static void start_transmit(_Types&&... args)
  {
    std::make_shared<transmit_session>(std::forward<_Types>(args)...)->start();
  }

public:
  transmit_session(const std::string& filename, std::function<int(yasio::sbyte_buffer, io_completion_cb_t)> send_cb, std::function<void()> complete_cb)
      : send_cb_(std::move(send_cb)), complete_cb_(std::move(complete_cb))
  {
    this->fp_ = posix_fopen(filename, "rb");
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
    else
    { // reply stats to client if open file failed
      int n     = sprintf(buffer_, "The commands processed totals=%lld", g_stats_hits);
      auto self = shared_from_this();
      send_cb_(yasio::sbyte_buffer(buffer_, buffer_ + n), [self](int, size_t) { self->complete_cb_(); });
    }
  }

  void handle_write()
  {
    auto n = fread(buffer_, 1, YASIO_ARRAYSIZE(buffer_), this->fp_);
    if (n > 0)
    {
      auto self = shared_from_this();
      if (send_cb_(yasio::sbyte_buffer(buffer_, buffer_ + n), std::bind(&transmit_session::handle_write, self)) < 0)
      {
        complete_cb_();
      }
    }
    else
    { // done
      complete_cb_();
    }
  }

  std::function<int(yasio::sbyte_buffer, io_completion_cb_t)> send_cb_;
  std::function<void()> complete_cb_;

  FILE* fp_ = nullptr;
  char buffer_[2 * 1024 * 1024];
};

////////////////////////// ftp_session /////////////////////////////////

namespace www
{
inline bool is_absolute_path(const std::string& path) { return *path.c_str() == '/'; }
inline void ensure_dir_slash(std::string& path)
{
  if (!path.empty() && path.back() != '/')
    path.push_back('/');
}
namespace nzls
{
template <typename _CStr, typename _Fn>
inline void fast_split_of(_CStr s, size_t slen, const typename std::remove_pointer<_CStr>::type* delims, _Fn func)
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
template <typename _Elem, typename _Fn>
inline void fast_split_of(cxx17::basic_string_view<_Elem> s, const _Elem* delims, _Fn func)
{
  return fast_split_of(s.data(), s.length(), delims, func);
}
} // namespace nzls
static bool verify_path(cxx17::string_view path, bool isdir)
{ // verify path to disallow access out of wwwroot on local filesystem
  int total = isdir ? 0 : -1;
  int upcnt = 0;
  nzls::fast_split_of(path, R"(/\)", [&](const char* s, const char* e, char) {
    ++total;
    if (e - s == 2 && *s == '.' && s[1] == '.')
      ++upcnt;
  });
  return upcnt <= (total / 2);
}
} // namespace www

ftp_session::ftp_session(ftp_server& server, transport_handle_t thandle, int transfer_cindex)
    : server_(server), thandle_ctl_(thandle), thandle_transfer_(nullptr), status_(transfer_status::NONE), transferring_(false)
{
  transfer_cindex_ = transfer_cindex;
  dir_             = "/"; // web current working directory
}

ftp_session::~ftp_session()
{
  if (expire_timer_)
    expire_timer_->cancel(*__service);

  printf("the ftp_session: %p destroyed!\n", this);
}

const std::string& ftp_session::to_fspath()
{
  this->fspath_ = __wwwroot;
  this->fspath_.append(this->dir_);
  fsutils::to_styled_path(this->fspath_);
  return this->fspath_;
}
const std::string& ftp_session::to_fspath(const std::string& param)
{
  this->fspath_ = __wwwroot;
  if (www::is_absolute_path(param))
    this->fspath_.append(param);
  else
    this->fspath_.append(this->dir_).append(param);
  fsutils::to_styled_path(this->fspath_);
  return this->fspath_;
}

// say hello to client, we can start ftp service
void ftp_session::say_hello()
{
  using namespace std; // for string literal operator 'sv'
  stock_reply(_mksv("220"), _mksv("x-studio Pro embedded FTP Server (c) 2022."), false);
  stock_reply(_mksv("220"), _mksv("Please visit https://x-studio.net/"));

  start_exprie_timer();
}

void ftp_session::start_exprie_timer()
{
  // kickout after 10 seconds, if no request from client
  if (expire_timer_)
  {
    expire_timer_->expires_from_now();
    expire_timer_->async_wait(*__service, create_timer_cb());
  }
  else
  {
    expire_timer_ = __service->schedule(std::chrono::seconds(10), create_timer_cb());
  }
}

timer_cb_t ftp_session::create_timer_cb()
{
  std::weak_ptr<ftp_session> this_wptr = shared_from_this();
  return [=](io_service&) {
    auto thiz = this_wptr.lock();
    if (thiz)
    {
      bool expired = expire_timer_->expired();
      if (expired && !thiz->transferring_)
      { // timeout
        if (thiz->thandle_ctl_)
        {
          printf("the connection: #%u is expired, close it!\n", thiz->thandle_ctl_->id());
          __service->close(thiz->thandle_ctl_);
          thiz->thandle_ctl_ = nullptr;
        }
      }
    }
    else
    {
      printf("the session is destroyed!\n");
    }
    return true;
  };
}

void ftp_session::handle_packet(yasio::io_packet& packet)
{
  cxx17::string_view algsv(packet.data(), packet.size());
  size_t crlf;
  while ((crlf = algsv.find_last_of("\r\n")) != cxx17::string_view::npos)
    algsv.remove_suffix(1);

  std::string cmd, param;
  auto offset = algsv.find_first_of(' ');
  if (offset != cxx17::string_view::npos)
  {
    cxx17::assign(cmd, algsv.substr(0, offset));
    offset = algsv.find_first_not_of(' ', offset + 1);
    if (offset != cxx17::string_view::npos)
      cxx17::assign(param, algsv.substr(offset));
  }
  else
    cxx17::assign(cmd, algsv);

  cmd.resize(sizeof(ftp_cmd_id_t));

  std::cout << algsv << "\n";
  auto handler_id = *reinterpret_cast<const uint32_t*>(cmd.c_str());
  auto it         = handlers_.find(handler_id);
  if (it != handlers_.end())
  {
    it->second(this, param);
    expire_timer_->cancel(*__service);
    start_exprie_timer();
  }
  else
  {
    using namespace std; // for string literal operator 'sv'
    stock_reply(_mksv("500"), _mksv("Syntax Error."));
  }
}

void ftp_session::open_transimt_session(transport_handle_t thandle)
{
  this->thandle_transfer_ = thandle;
  do_transmit();
}

/// ---------- All supported commands handlers ------------
void ftp_session::process_USER(const std::string& /*param*/) { stock_reply(_mksv("230"), _mksv("Login successful.")); }
void ftp_session::process_PASS(const std::string& /*param*/) { stock_reply(_mksv("230"), _mksv("Login successful.")); }
void ftp_session::process_SYST(const std::string& /*param*/)
{ // The firefox will check the system type
  stock_reply(_mksv("215"), _mksv("WINDOWS"));
}
void ftp_session::process_PWD(const std::string& /*param*/) { stock_reply(_mksv("215"), this->dir_, true, true); }
void ftp_session::process_TYPE(const std::string& mode)
{
  stock_reply(_mksv("200"), mode == "I" ? _mksv("Switching to Binary mode.") : _mksv("Switching to ASCII mode."));
}
void ftp_session::process_SIZE(const std::string& param)
{
  bool isdir = false;
  auto size  = fsutils::get_file_size(to_fspath(param), isdir);
  if (size > 0)
  {
    stock_reply(_mksv("213"), std::to_string(size));
  }
  else
  {
    stock_reply(_mksv("550"), isdir ? _mksv("not a plain file.") : _mksv("No such file or directory."));
  }
}
void ftp_session::process_CDUP(const std::string& /*param*/) { process_CWD(".."); }
void ftp_session::process_CWD(const std::string& param)
{
  std::string path = dir_;
  if (param == "..")
  {
    if (path.size() > 1)
    {
      path.resize(path.length() - 1); // remove the dir last slash
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
    if (www::is_absolute_path(param))
      path = param;
    else
      path.append(param);
  }
  www::ensure_dir_slash(path);
  if (www::verify_path(path, true))
  {
    if (fsutils::is_dir_exists(posix_upath(to_fspath(path))))
    {
      this->dir_ = path;
      stock_reply(_mksv("250"), _mksv("OK."));
    }
    else
      stock_reply(_mksv("550"), yasio::strfmt(127, "CWD failed, \"%s\": directory not found.", param.c_str()));
  }
  else
    stock_reply(_mksv("550"), yasio::strfmt(127, "CWD failed, \"%s\": directory invalid.", param.c_str()));
}
void ftp_session::process_PASV(const std::string& /*param*/)
{
  static u_short listening_port = 20525;

  int cindex   = transfer_cindex_;
  auto channel = __service->channel_at(cindex);
  if (channel != nullptr)
  {
    if (!__service->is_open(cindex))
    {
      ++listening_port;
      __service->set_option(YOPT_C_REMOTE_PORT, cindex, listening_port);
      __service->set_option(YOPT_C_MOD_FLAGS, cindex, YCF_REUSEADDR, 0);
      __service->open(cindex, YCK_TCP_SERVER);
    }

    std::string msg = "Entering passive mode ";
    ip::endpoint ep;
    if (__wanip.empty() && thandle_ctl_)
      ep.as_in(thandle_ctl_->local_endpoint().ip().c_str(), channel->remote_port());
    else
      ep.as_in(__wanip.c_str(), channel->remote_port());
    msg += ep.format_v4("(%N,%H,%L,%M,%l,%h).");
    stock_reply("227", msg);
  }
  else
  { // not channel to transfer data
  }
}
void ftp_session::process_EPSV(const std::string& /*param*/) { stock_reply(_mksv("522"), _mksv("EPSV Unsupported.")); }
void ftp_session::process_LIST(const std::string& /*param*/)
{
  stock_reply(_mksv("150"), _mksv("Sending directory listing."));
  status_ = ftp_session::transfer_status::LIST;
  to_fspath();
  this->do_transmit();
}
void ftp_session::process_RETR(const std::string& param)
{
  std::string path = www::is_absolute_path(param) ? param : dir_ + param;
  if (www::verify_path(path, false))
  {
    if (fsutils::is_file_exists(posix_upath(to_fspath(path))))
    {
      stock_reply(_mksv("150"), _mksv("Opening BINARY mode for file transfer."));
      status_ = ftp_session::transfer_status::FILE;
      this->do_transmit();
      return;
    }
  }

  stock_reply(_mksv("550"), _mksv("No such file or directory."));
}
void ftp_session::process_QUIT(const std::string& /*param*/) { stock_reply(_mksv("221"), _mksv("Bye.")); }
void ftp_session::process_AUTH(const std::string& param)
{
  stock_reply(_mksv("502"), yasio::strfmt(127, "Explicit %s authentication not allowed.", param.c_str()));
}
void ftp_session::process_OPTS(const std::string& /*param*/) { stock_reply(_mksv("211"), _mksv("Always in UTF8 mode.")); }
void ftp_session::process_FEAT(const std::string& /*param*/)
{
  stock_reply(_mksv("211"), _mksv("Features:"), false);
  stock_reply(_mksv(""), _mksv("UTF8"), false);
  stock_reply(_mksv("211"), _mksv("End"));
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
      gmtime_r(&tval, &daytm);

      fsutils::list_files(this->fspath_, [&](tinydir_file& f) {
        obs.write_bytes(f.is_dir ? _mksv("dr--r--r--") : _mksv("-r--r--r--"));
        obs.write_bytes(f.is_dir ? _mksv(" 2 0 0") : _mksv(" 1 0 0"));
        posix_stat_st st;
        if (0 == posix_ustat(f.path, &st))
        {
          struct tm tinfo;
          gmtime_r(&st.st_mtime, &tinfo);

          if (st.st_mode & S_IFREG)
          {
            obs.write_byte(' ');
            obs.write_bytes(std::to_string(st.st_size));
          }
          else
            obs.write_bytes(_mksv(" 0"));

          char buf[96];
#if defined(_MSC_VER) && _MSC_VER < 1900
          strftime(buf, 96, tinfo.tm_year == daytm.tm_year ? " %b %d %H:%M" : " %b %d %Y", &daytm);
#else
          strftime(buf, 96, tinfo.tm_year == daytm.tm_year ? " %b  %e  %R" : " %b  %e  %Y", &tinfo);
#endif
          obs.write_bytes(buf);
        }

        obs.write_byte(' ');
        obs.write_bytes(posix_upath(f.name));
        obs.write_byte('\n');
      });

      if (!obs.empty())
        __service->write(this->thandle_transfer_, std::move(obs.buffer()), [=](int, size_t) { stock_reply(_mksv("226"), _mksv("Done.")); });
      else
        stock_reply(_mksv("226"), _mksv("Done."));
    }
    else if (this->status_ == transfer_status::FILE)
    {
      printf("FTP-DATA: start transfer file: %s...\n", this->fspath_.c_str());
      transferring_ = true;
      transmit_session::start_transmit(
          this->fspath_,
          [=](yasio::sbyte_buffer buffer, std::function<void(int, size_t)> handler) {
            return __service->write(this->thandle_transfer_, std::move(buffer), std::move(handler));
          },
          [=]() {
            stock_reply(_mksv("226"), _mksv("Done."));
            transferring_ = false;
          });
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

void ftp_session::stock_reply(cxx17::string_view code, cxx17::string_view resp_data, bool finished, bool ispath)
{
  if (!this->thandle_ctl_)
    return;
  printf("Reponse:%s, msg: %s\n", code.data(), resp_data.data());

  obstream obs;

  using namespace std; // for string literal operator 'sv'
  if (!code.empty())
  {

    obs.write_bytes(code);
    obs.write_byte(finished ? ' ' : '-');
    if (ispath)
      obs.write_byte('\"');
    obs.write_bytes(resp_data);
    if (ispath)
      obs.write_byte('\"');
    obs.write_bytes(_mksv("\r\n"));
  }
  else
  {
    obs.write_bytes(resp_data);
    obs.write_bytes(_mksv("\r\n"));
  }

  if (code == "226")
    this->status_ = transfer_status::NONE;

  __service->write(this->thandle_ctl_, std::move(obs.buffer()), [=](int, size_t) {
    if (code == "226" && this->thandle_transfer_ != nullptr)
    {
      __service->close(this->thandle_transfer_);
      this->thandle_transfer_ = nullptr;
    }
  });
}

// register all supported commands' handler
void ftp_session::register_handlers_once()
{
#define XSFTPD_REGISTER(cmd) register_handler(#cmd, std::bind(&ftp_session::process_##cmd, std::placeholders::_1, std::placeholders::_2));
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
    XSFTPD_REGISTER(EPSV);
    XSFTPD_REGISTER(LIST);
    XSFTPD_REGISTER(RETR);
    XSFTPD_REGISTER(QUIT);
    XSFTPD_REGISTER(AUTH);
    XSFTPD_REGISTER(OPTS);
    XSFTPD_REGISTER(FEAT);
  }
}

void ftp_session::register_handler(std::string cmd, std::function<void(ftp_session*, const std::string&)> handler)
{
  cmd.resize(sizeof(ftp_cmd_id_t));
  handlers_.emplace(*reinterpret_cast<const uint32_t*>(cmd.c_str()), std::move(handler));
}

std::unordered_map<ftp_cmd_id_t, std::function<void(ftp_session*, const std::string&)>> ftp_session::handlers_;
