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

  int transfer_start_id = static_cast<int>(hosts.size());
  for (auto i = transfer_start_id; i < transfer_start_id + max_clients; ++i)
  {
    hosts.push_back({"0.0.0.0", 0});
    this->avails_.push_back(i);
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
          {
            on_open_session(ev);
          }
          else
          {
            on_open_transmit_session(ev);
          }
        }
        break;
      case YEK_CONNECTION_LOST:
        if (ev->cindex() == 0)
        {
          on_close_session(ev);
        }
        break;
    }
  });
}

void ftp_server::on_open_session(event_ptr& ev)
{
  auto thandle = ev->transport();
  if (!this->avails_.empty())
  {
    auto cindex       = this->avails_.back();
    ev->transport_ud(cindex);
    auto result = this->sessions_.emplace(cindex, std::make_shared<ftp_session>(*this, ev));
    if (result.second)
    {
      printf("a ftp session:%p income, cindex=%d\n", thandle, cindex);
      this->avails_.pop_back();
      result.first->second->say_hello();
    }
    else
    {
      assert(false);
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
  auto it = this->sessions_.find(ev->transport_ud<int>());
  if (it != this->sessions_.end())
  {
    printf("the ftp session:%p is ended, cindex=%d\n", ev->transport(), it->first);
    this->avails_.push_back(it->first);
    this->sessions_.erase(it);
  }
}

void ftp_server::on_open_transmit_session(event_ptr& ev)
{
  auto cindex = ev->cindex();
  auto thandle = ev->transport();
  auto it = this->sessions_.find(cindex);
  if (it != this->sessions_.end())
  {
    it->second->open_transimt_session(thandle);
  }
  else
  {
    printf("Error: no ftp session for file transfer channel: %d\n", cindex);
    service_->close(thandle);
  }
}

void ftp_server::dispatch_packet(event_ptr& ev)
{
  auto it = this->sessions_.find(ev->transport_ud<int>());
  if (it != this->sessions_.end())
  {
    it->second->handle_packet(ev->packet());
  }
  else
  {
    auto thandle = ev->transport();
    printf("Error: cann't dispatch for a unregistered session: %p, will close it.", thandle);
    service_->close(thandle);
  }
}
