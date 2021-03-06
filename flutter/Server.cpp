#include "flutter/Server.h"

#include <json/json.hpp>

#include <common/Logger.h>
#include <event/Trigger.h>
#include <networking/Messenger.h>
#include <stats/StatsManager.h>

#include <algorithm>

namespace flutter {

using json = nlohmann::json;

class Server::Session {
public:
  Session(event::EventLoop& loop, std::unique_ptr<networking::TCPSocket> client)
      : messenger_(new networking::Messenger(loop, std::move(client))),
        didEnd_(loop.createBaseCondition()) {
    loop.arm("flutter::Server::Session::messengerDisconnectedTrigger",
             {messenger_->didDisconnect()}, [this]() { didEnd_->fire(); });
  }

  void publish(stats::StatsManager::SubscribeData const& data) {
    if (didEnd_->eval()) {
      return;
    }

    if (!messenger_->outboundQ->canPush()->eval()) {
      LOG_I("Flutter") << "Dropping a data message due to full Messenger queue."
                       << std::endl;
      return;
    }

    json payload = json::array();

    for (auto const& entry : data) {
      payload.push_back({
          {"entity", entry.first.first},
          {"metric", entry.first.second},
          {"value", entry.second},
      });
    }

    messenger_->outboundQ->push(networking::Message("data", payload));
  }

  event::Condition* didEnd() const { return didEnd_.get(); }

private:
  std::unique_ptr<networking::Messenger> messenger_;
  std::unique_ptr<event::BaseCondition> didEnd_;

private:
  Session(Session const& copy) = delete;
  Session& operator=(Session const& copy) = delete;

  Session(Session&& move) = delete;
  Session& operator=(Session&& move) = delete;
};

Server::Server(event::EventLoop& loop, ServerConfig config) : loop_(loop) {
  socket_.reset(new networking::TCPServer(loop, networking::NetworkType::IPv4));
  socket_->bind(config.port);

  acceptor_ =
      loop.createAction("flutter::Server::acceptor_", {socket_->canAccept()});
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
  sessions_.emplace_back(new Session(loop_, std::move(client)));

  loop_.arm("flutter::Server::removeSessionTrigger",
            {sessions_.back()->didEnd()},
            [session = sessions_.back().get(), this]() {
              auto it = std::find_if(
                  sessions_.begin(), sessions_.end(),
                  [session](auto const& ptr) { return ptr.get() == session; });

              assertTrue(it != sessions_.end(),
                         "Cannot find flutter server session to remove.");
              sessions_.erase(it);
            });
}
} // namespace flutter
