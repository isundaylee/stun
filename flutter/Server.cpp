#include "flutter/Server.h"

#include <common/Logger.h>
#include <stats/StatsManager.h>

namespace flutter {

class Server::Session {
public:
  Session(Server* server, std::unique_ptr<networking::TCPSocket> client)
      : server_(server), client_(std::move(client)) {}

  void publish(stats::StatsManager::SubscribeData const& data) {
    char const* hello = "hello";
    client_->write((Byte*)hello, 5);
  }

private:
  Server* server_;
  std::unique_ptr<networking::TCPSocket> client_;

private:
  Session(Session const& copy) = delete;
  Session& operator=(Session const& copy) = delete;

  Session(Session&& move) = delete;
  Session& operator=(Session&& move) = delete;
};

Server::Server(ServerConfig config) : config_(config) {
  socket_.reset(new networking::TCPServer());
  socket_->bind(config.port);

  acceptor_.reset(new event::Action({socket_->canAccept()}));
  acceptor_->callback.setMethod<Server, &Server::doAccept>(this);

  stats::StatsManager::subscribe([this](auto const& data) {
    for (auto const& session : sessions_) {
      session->publish(data);
    }
  });

  LOG_I("Flutter") << "Flutter server listening on port " << config.port
                   << std::endl;
}

Server::~Server() = default;

void Server::doAccept() {
  auto client = std::make_unique<networking::TCPSocket>(socket_->accept());
  sessions_.emplace_back(new Session(this, std::move(client)));
}
}
