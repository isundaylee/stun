#pragma once

#include <common/Configerator.h>

#include <stun/SessionHandler.h>

#include <networking/IPAddressPool.h>
#include <networking/InterfaceConfig.h>
#include <networking/Messenger.h>
#include <networking/TCPPipe.h>
#include <networking/Tunnel.h>
#include <networking/UDPPipe.h>

#include <memory>
#include <string>
#include <vector>

namespace stun {

using namespace networking;

class SessionHandler;

class CommandCenter {
public:
  CommandCenter() {}

  void serve(int port);
  void connect(std::string const& host, int port);

  std::unique_ptr<IPAddressPool> addrPool;

private:
  static int numClients;

  void handleAccept(TCPPipe&& client);

  std::unique_ptr<TCPPipe> commandServer_;
  std::vector<SessionHandler> servers_;
  std::unique_ptr<SessionHandler> client_;
};
}
