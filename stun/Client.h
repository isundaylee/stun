#pragma once

#include <event/EventLoop.h>

#include <stun/ClientSessionHandler.h>

namespace stun {

class Client {
public:
#if TARGET_IOS
  Client(event::EventLoop& loop, ClientConfig config,
         ClientSessionHandler::TunnelFactory tunnelFactory);
#else
  Client(event::EventLoop& loop, ClientConfig config);
#endif
  ~Client();

private:
  void connect();

private:
  Client(Client const& copy) = delete;
  Client& operator=(Client const& copy) = delete;

  Client(Client&& move) = delete;
  Client& operator=(Client&& move) = delete;

  event::EventLoop& loop_;

  ClientConfig config_;
  
  ClientSessionHandler::TunnelFactory tunnelFactory_;

  std::unique_ptr<ClientSessionHandler> handler_;
  std::unique_ptr<event::Action> reconnector_;
  std::unique_ptr<event::BaseCondition> cleanerDidFinish_;
  std::unique_ptr<event::Action> cleaner_;

  static void createRoutes(std::vector<networking::Route> routes);
  std::unique_ptr<Tunnel> createTunnel(ClientTunnelConfig config);
  void doReconnect();
};
} // namespace stun
