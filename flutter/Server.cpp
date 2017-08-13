#include "flutter/Server.h"

#include <flutter/protobuf/flutter.pb.h>

#include <common/Logger.h>
#include <event/Trigger.h>
#include <stats/StatsManager.h>

#include <algorithm>

namespace flutter {

static const size_t kFlutterServerMessageBufferSize = 1024;

class Server::Session {
public:
  Session(std::unique_ptr<networking::TCPSocket> client)
      : client_(std::move(client)), didEnd_(new event::BaseCondition()) {}

  void publish(stats::StatsManager::SubscribeData const& data) {
    if (didEnd_->eval()) {
      return;
    }

    flutter::protobuf::Data protoData;
    for (auto const& entry : data) {
      flutter::protobuf::DataPoint* protoPoint = protoData.add_points();
      protoPoint->set_entity(entry.first.first);
      protoPoint->set_metric(entry.first.second);
      protoPoint->set_value(entry.second);
    }

    Byte message[kFlutterServerMessageBufferSize];
    uint32_t protoSize = protoData.ByteSizeLong();
    *((uint32_t*)message) = htonl(protoSize);
    uint32_t headerSize = sizeof(uint32_t);
    bool ret = protoData.SerializeToArray(message + headerSize,
                                          sizeof(message) - headerSize);
    assertTrue(ret, "Flutter protobuf serialization failed.");

    try {
      size_t messageSize = headerSize + protoSize;
      size_t written = client_->write(message, messageSize);
      assertTrue(written == messageSize, "Flutter data fragmented.");
    } catch (networking::SocketClosedException const& ex) {
      didEnd_->fire();
    }
  }

  event::Condition* didEnd() const { return didEnd_.get(); }

private:
  std::unique_ptr<networking::TCPSocket> client_;
  std::unique_ptr<event::BaseCondition> didEnd_;

private:
  Session(Session const& copy) = delete;
  Session& operator=(Session const& copy) = delete;

  Session(Session&& move) = delete;
  Session& operator=(Session&& move) = delete;
};

Server::Server(ServerConfig config) {
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
  sessions_.emplace_back(new Session(std::move(client)));

  event::Trigger::arm(
      {sessions_.back()->didEnd()},
      [ session = sessions_.back().get(), this ]() {
        auto it = std::find_if(
            sessions_.begin(), sessions_.end(),
            [session](auto const& ptr) { return ptr.get() == session; });

        assertTrue(it != sessions_.end(),
                   "Cannot find flutter server session to remove.");
        sessions_.erase(it);
      });
}
}
