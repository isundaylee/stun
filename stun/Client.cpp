#include "stun/Client.h"

#include <event/Action.h>
#include <event/Trigger.h>

namespace stun {

const int kReconnectDelayInterval = 5000 /* ms */;

Client::Client(ClientConfig config) : config_(config) { connect(); }

void Client::connect() {
  auto socket = TCPSocket{};
  socket.connect(config_.serverAddr);

  handler_.reset(new ClientSessionHandler(
      config_, std::make_unique<TCPSocket>(std::move(socket))));
  reconnector_.reset(new event::Action({handler_->didEnd()}));
  reconnector_->callback.setMethod<Client, &Client::doReconnect>(this);
}

void Client::doReconnect() {
  handler_.reset();
  reconnector_.reset();

  LOG_I("Client") << "Will reconnect in " << kReconnectDelayInterval << " ms."
                  << std::endl;

  event::Trigger::performIn(kReconnectDelayInterval, [this]() {
    LOG_I("Client") << "Reconnecting..." << std::endl;
    connect();
  });
}
}
