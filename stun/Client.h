#pragma once

#include <stun/ClientSessionHandler.h>

namespace stun {

class Client {
public:
#if IOS
  Client(ClientConfig config,
         ClientSessionHandler::TunnelFactory tunnelFactory);
#else
  Client(ClientConfig config);
#endif
  ~Client();

private:
  ClientConfig config_;

  void connect();

private:
  Client(Client const& copy) = delete;
  Client& operator=(Client const& copy) = delete;

  Client(Client&& move) = delete;
  Client& operator=(Client&& move) = delete;

#if IOS
  ClientSessionHandler::TunnelFactory tunnelFactory_;
#endif

  std::unique_ptr<ClientSessionHandler> handler_;
  std::unique_ptr<event::Action> reconnector_;

  static void createRoutes(std::vector<networking::Route> routes);
  std::unique_ptr<Tunnel> createTunnel(ClientTunnelConfig config);
  void doReconnect();
};
}
