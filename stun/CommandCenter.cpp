#include "stun/CommandCenter.h"

#include <stun/SessionHandler.h>

#include <common/Util.h>
#include <common/Configerator.h>
#include <networking/Messenger.h>

#include <algorithm>

namespace stun {

using namespace networking;

int CommandCenter::numClients = 0;

void CommandCenter::serve(int port) {
  SubnetAddress addrPoolAddr(common::Configerator::getString("address_pool"));
  addrPool.reset(new IPAddressPool(addrPoolAddr));

  commandServer_.reset(new TCPPipe());
  commandServer_->onAccept = [this](TCPPipe&& client) {
    handleAccept(std::move(client));
  };

  commandServer_->setName("CENTER");
  commandServer_->open();
  commandServer_->bind(port);
}

void CommandCenter::handleAccept(TCPPipe&& client) {
  size_t clientIndex = numClients;
  numClients++;

  client.onClose = [this, clientIndex]() {
    auto it = std::find_if(servers_.begin(), servers_.end(),
                           [clientIndex](SessionHandler const& server) {
                             return server.clientIndex == clientIndex;
                           });

    assertTrue(it != servers_.end(), "Cannot find the client to remove.");
    servers_.erase(it);
  };

  client.setName("COMMAND-" + std::to_string(clientIndex));
  servers_.emplace_back(this, true, "", clientIndex, std::move(client));
  servers_.back().start();
}

void CommandCenter::connect(std::string const& host, int port) {
  TCPPipe toServer;
  toServer.setName("COMMAND");
  toServer.open();
  toServer.connect(host, port);

  client_.reset(new SessionHandler(this, false, host, 0, std::move(toServer)));
  client_->start();
}
}
