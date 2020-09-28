/// very simple ftp server by yasio, only support file retrive command

#include "ftp_server.hpp"

#define FTP_CONTROL_CHANNEL_INDEX 0

ftp_server::ftp_server(cxx17::string_view root, cxx17::string_view wanip)
{
  ftp_session::register_handlers_once();
  cxx17::assign(root_, root);

  if (!wanip.empty())
  {
    std::vector<ip::endpoint> eps;
    xxsocket::resolve(eps, wanip.data());
    if (!eps.empty())
    {
      wanip_ = eps[0].ip();
      YASIO_LOG("resolve host: %s succeed, ip: %s", wanip.data(), wanip_.c_str());
    }
  }
  else
    cxx17::assign(wanip_, wanip);
}
void ftp_server::run(int max_clients, u_short port)
{
  // max support 10 clients
  std::vector<io_hostent> hosts{{"0.0.0.0", 21}};

  transfer_start_index_ = static_cast<int>(hosts.size());
  for (auto i = transfer_start_index_; i < transfer_start_index_ + max_clients; ++i)
  {
    hosts.push_back({"0.0.0.0", 0});
    this->avails_.push_back(i);
    this->sessions_.push_back(ftp_session_ptr{});
  }
  service_.reset(new io_service(&hosts.front(), static_cast<int>(hosts.size())));

  service_->set_option(YOPT_S_NO_NEW_THREAD, 1);
  service_->set_option(YOPT_S_DEFERRED_EVENT, 0);
  service_->set_option(YOPT_C_MOD_FLAGS, FTP_CONTROL_CHANNEL_INDEX, YCF_REUSEADDR, 0);

  service_->schedule(std::chrono::microseconds(1), [=]() {
    service_->open(FTP_CONTROL_CHANNEL_INDEX, YCK_TCP_SERVER);
    return true;
  });

  service_->start([=](event_ptr&& ev) {
    auto thandle = ev->transport();
    switch (ev->kind())
    {
      case YEK_PACKET:
        if (ev->cindex() == 0)
        {
          dispatch_packet(ev);
        }
        else
        {
          ; // ignore
        }
        break;
      case YEK_CONNECT_RESPONSE:
        if (ev->status() == 0)
        {
          if (ev->cindex() == 0)
            on_open_session(ev); // port 21
          else
            on_open_transmit_session(ev); // data port
        }
        break;
      case YEK_CONNECTION_LOST:
        if (ev->cindex() == 0)
          on_close_session(ev);
        break;
    }
  });
}

void ftp_server::on_open_session(event_ptr& ev)
{
  auto thandle = ev->transport();
  if (!this->avails_.empty())
  {
    auto transfer_cindex       = this->avails_.back();
    auto wrap            = new (std::nothrow) ftp_session_ptr(std::make_shared<ftp_session>(*this, thandle, transfer_cindex));
    if (wrap)
    {
      this->sessions_[to_session_index(transfer_cindex)] = *wrap;
      ev->transport_udata(wrap); // store ftp_session_ptr wrap to as transport userdata

      printf("a ftp session: %p income, transfer_cindex=%d\n", thandle, transfer_cindex);
      this->avails_.pop_back();
      (*wrap)->say_hello();
    }
    else
    {
      service_->close(thandle);
    }
  }
  else
  { // close directly
    service_->close(thandle);
  }
}

void ftp_server::on_close_session(event_ptr& ev)
{
  auto wrap = ev->transport_udata<ftp_session_ptr*>();
  if (wrap)
  {
    auto transfer_cindex = (*wrap)->transfer_cindex_;
    printf("the ftp session: %p is ended, transfer_cindex=%d\n", ev->transport(), transfer_cindex);
    this->avails_.push_back(transfer_cindex); // recyle transfer channel
    this->sessions_[to_session_index(transfer_cindex)].reset(); // release the session
    delete wrap; // delete wrap session object
  }
}

void ftp_server::on_open_transmit_session(event_ptr& ev)
{
   auto cindex = ev->cindex();
   auto thandle = ev->transport();
   auto session = this->sessions_[to_session_index(cindex)];
   if (session)
     session->open_transimt_session(thandle);
   else
   {
     printf("Error: no ftp session for file transfer channel: %d\n", cindex);
     service_->close(thandle);
   }
}

void ftp_server::dispatch_packet(event_ptr& ev)
{
  auto wrap = ev->transport_udata<ftp_session_ptr*>();
  if (wrap)
    (*wrap)->handle_packet(ev->packet());
  else
  {
    auto thandle = ev->transport();
    printf("Error: cann't dispatch for a unregistered session: %p, will close it.", thandle);
    service_->close(thandle);
  }
}
