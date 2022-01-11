#pragma once

#include "yasio/yasio.hpp"
#include "fsutils.hpp"
#include <unordered_set>
#include <unordered_map>

using namespace yasio;

typedef uint32_t ftp_cmd_id_t;
class ftp_server;
class ftp_session : public std::enable_shared_from_this<ftp_session>
{
  friend class ftp_server;
  enum class transfer_status
  {
    NONE = 0,
    LIST = 1,
    FILE,
  };

public:
  /// <summary>
  /// ftp_session constructor
  /// </summary>
  /// <param name="server">ftp_server</param>
  /// <param name="thandle">transport handle for control commands</param>
  /// <param name="transfer_cindex">channel index to transfer data</param>
  ftp_session(ftp_server& server, transport_handle_t thandle, int transfer_cindex);
  ~ftp_session();

  // say hello to client, we can start ftp service
  void say_hello();
  void start_exprie_timer();
  timer_cb_t create_timer_cb();
  void handle_packet(yasio::io_packet& packet);
  void stock_reply(cxx17::string_view code, cxx17::string_view resp_data, bool finished = true,
                   bool ispath = false);

  void open_transimt_session(transport_handle_t thandle);

  /// ---------- All supported commands handlers ------------
  void process_USER(const std::string& param);
  void process_PASS(const std::string& param);
  void process_SYST(const std::string& param);
  void process_PWD(const std::string& param);
  void process_TYPE(const std::string& param);
  void process_SIZE(const std::string& param);
  void process_CDUP(const std::string& /*param*/);
  void process_CWD(const std::string& param);
  void process_PASV(const std::string& param);
  void process_LIST(const std::string& param);
  void process_RETR(const std::string& param);
  void process_QUIT(const std::string& param);
  void process_AUTH(const std::string& param);
  void process_OPTS(const std::string& param);
  void process_FEAT(const std::string& param);

  // register all supported commands' handler
  static void register_handlers_once();

  static void register_handler(std::string cmd,
                               std::function<void(ftp_session*, const std::string&)> handler);

  void do_transmit();

  const std::string& to_fspath();
  const std::string& to_fspath(const std::string& param);

private:
  ftp_server& server_;
  transport_handle_t thandle_ctl_;
  transport_handle_t thandle_transfer_;
  transfer_status status_;

  int transfer_cindex_;

  // web working directory, must be always ends with '/'
  std::string dir_; 

  // cache filesystem path for LIST and FILE transfer
  std::string fspath_; 

  deadline_timer_ptr expire_timer_;

  bool transferring_;

  static std::unordered_map<ftp_cmd_id_t, std::function<void(ftp_session*, const std::string&)>>
      handlers_;
};

typedef std::shared_ptr<ftp_session> ftp_session_ptr;
