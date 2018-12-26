#pragma once

#include <event/Action.h>
#include <networking/TCPServer.h>

namespace flutter {

struct ServerConfig {
public:
  int port;
};

class Server {
public:
  Server(event::EventLoop& loop, ServerConfig config);
  ~Server();

private:
  event::EventLoop& loop_;

  std::unique_ptr<networking::TCPServer> socket_;
  std::unique_ptr<event::Action> acceptor_;

  class Session;
  std::vector<std::unique_ptr<Session>> sessions_;

  void doAccept();

private:
  Server(Server const& copy) = delete;
  Server& operator=(Server const& copy) = delete;

  Server(Server&& move) = delete;
  Server& operator=(Server&& move) = delete;
};
} // namespace flutter
