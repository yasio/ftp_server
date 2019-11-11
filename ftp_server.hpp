#pragma once
#include "ftp_session.hpp"

class ftp_server
{
  friend class ftp_session;

public:
  ftp_server(cxx17::string_view root, cxx17::string_view wanip);
  void run(int max_clients = 10, u_short port = 21);

  void on_open_session(transport_handle_t thandle);

  void on_close_session(transport_handle_t thandle);

  void on_open_transmit_session(int cindex, transport_handle_t thandle);
  void dispatch_packet(transport_handle_t thandle, std::vector<char>&& packet);

private:
  io_service service_;

  // the avaiable session_id, session id is channle index
  std::vector<int> avails_;
  std::unordered_map<int, ftp_session_ptr> sessions_;

  std::string root_;
  std::string wanip_;
};
