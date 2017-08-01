#include "stun/CommandCenter.h"

#include <stun/SessionHandler.h>

#include <common/Configerator.h>
#include <common/Util.h>
#include <event/Trigger.h>
#include <networking/Messenger.h>

#include <algorithm>

namespace stun {

using namespace networking;

int CommandCenter::numClients = 0;

CommandCenter::CommandCenter() : didDisconnect_(new event::BaseCondition()) {}

event::Condition* CommandCenter::didDisconnect() const {
  return didDisconnect_.get();
}

void CommandCenter::serve(int port) {
  SubnetAddress addrPoolAddr(common::Configerator::getString("address_pool"));
  addrPool.reset(new IPAddressPool(addrPoolAddr));

  commandServer_.reset(new TCPPipe());
  commandServer_->onAccept = [this](TCPPipe&& client) {
    handleAccept(std::move(client));
  };

  commandServer_->setName("Center");
  commandServer_->open();
  commandServer_->bind(port);
}

void CommandCenter::handleAccept(TCPPipe&& client) {
  size_t clientIndex = numClients;
  numClients++;

  client.setName("Command " + std::to_string(clientIndex));
  std::unique_ptr<SessionHandler> handler{
      new SessionHandler(this, true, "", clientIndex, std::move(client))};

  // Trigger to remove finished clients
  event::Trigger::arm(
      {handler->didEnd()},
      [this, clientIndex]() {
        auto it = std::find_if(
            servers_.begin(), servers_.end(),
            [clientIndex](std::unique_ptr<SessionHandler> const& server) {
              return server->clientIndex == clientIndex;
            });

        assertTrue(it != servers_.end(), "Cannot find the client to remove.");
        addrPool->release((*it)->myTunnelAddr);
        addrPool->release((*it)->peerTunnelAddr);
        servers_.erase(it);
      });

  servers_.push_back(std::move(handler));
  servers_.back()->start();
}

void CommandCenter::connect(std::string const& host, int port) {
  TCPPipe toServer;
  toServer.setName("Command");
  toServer.open();
  toServer.connect(host, port);

  didDisconnect_->arm();
  std::unique_ptr<SessionHandler> handler{
      new SessionHandler(this, false, host, 0, std::move(toServer))};

  event::Trigger::arm({handler->didEnd()},
                      [this]() {
                        LOG_T("Command") << "We are disconnected." << std::endl;
                        didDisconnect_->fire();
                        client_.reset();
                      });

  client_ = std::move(handler);
  client_->start();
}
}
