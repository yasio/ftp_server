#pragma once
#include "ftp_session.hpp"

class ftp_server
{
  friend class ftp_session;

public:
  ftp_server(cxx17::string_view root, cxx17::string_view wanip = "");
  void run(int max_clients = 10, u_short port = 21);

  void on_open_session(event_ptr& ev);

  void on_close_session(event_ptr& ev);

  void on_open_transmit_session(event_ptr& ev);
  void dispatch_packet(event_ptr& ev);

  int to_session_index(int transfer_index) {
    assert(transfer_index >= transfer_start_index_);
    auto session_index = transfer_index - transfer_start_index_;
    assert(session_index < this->sessions_.size());
    return session_index;
  }

private:
  std::unique_ptr<io_service> service_;

  // the avaiable session_id, session id is channle index
  std::vector<int> avails_;

  int transfer_start_index_;
  std::vector<ftp_session_ptr> sessions_; // session array, fixed==max_clients

  std::string root_;
  std::string wanip_;
};
