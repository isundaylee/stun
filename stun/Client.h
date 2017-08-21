#pragma once

#include <stun/ClientSessionHandler.h>

namespace stun {

class Client {
public:
  Client(ClientConfig config);
  ~Client();

private:
  ClientConfig config_;

  void connect();

private:
  Client(Client const& copy) = delete;
  Client& operator=(Client const& copy) = delete;

  Client(Client&& move) = delete;
  Client& operator=(Client&& move) = delete;

  std::unique_ptr<ClientSessionHandler> handler_;
  std::unique_ptr<event::Action> reconnector_;

  void doReconnect();
};
}
