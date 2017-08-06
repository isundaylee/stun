#include "stun/CommandCenter.h"

#include <stun/SessionHandler.h>

#include <common/Configerator.h>
#include <common/Util.h>
#include <event/Trigger.h>
#include <networking/Messenger.h>

#include <algorithm>

namespace stun {

using namespace networking;

CommandCenter::CommandCenter() : didDisconnect_(new event::BaseCondition()) {}

event::Condition* CommandCenter::didDisconnect() const {
  return didDisconnect_.get();
}

void CommandCenter::serve(int port) {
  SubnetAddress addrPoolAddr(common::Configerator::getString("address_pool"));
  addrPool.reset(new IPAddressPool(addrPoolAddr));

  server_.reset(new TCPServer());
  listener_.reset(new event::Action({server_->canAccept()}));
  listener_->callback.setMethod<CommandCenter, &CommandCenter::doAccept>(this);
  server_->bind(port);
}

void CommandCenter::doAccept() {
  TCPSocket client = server_->accept();

  LOG_I("Center") << "Accepted a client from "
                  << client.getPeerAddress().getHost() << std::endl;

  std::unique_ptr<SessionHandler> handler{new SessionHandler(
      this, Server, "", std::make_unique<TCPSocket>(std::move(client)))};

  // Trigger to remove finished clients
  auto handlerPtr = handler.get();
  event::Trigger::arm({handler->didEnd()}, [this, handlerPtr]() {
    auto it = std::find_if(serverHandlers_.begin(), serverHandlers_.end(),
                           [handlerPtr](auto const& handler) {
                             return handler.get() == handlerPtr;
                           });

    assertTrue(it != serverHandlers_.end(),
               "Cannot find the client to remove.");
    addrPool->release((*it)->myTunnelAddr);
    addrPool->release((*it)->peerTunnelAddr);
    serverHandlers_.erase(it);
  });

  serverHandlers_.push_back(std::move(handler));
}

void CommandCenter::connect(std::string const& host, int port) {
  TCPSocket client;
  client.connect(SocketAddress(host, port));

  didDisconnect_->arm();
  std::unique_ptr<SessionHandler> handler{new SessionHandler(
      this, Client, host, std::make_unique<TCPSocket>(std::move(client)))};

  event::Trigger::arm({handler->didEnd()}, [this]() {
    LOG_I("Command") << "We are disconnected." << std::endl;
    didDisconnect_->fire();
    clientHandler_.reset();
  });

  clientHandler_ = std::move(handler);
}
}
