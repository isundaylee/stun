#pragma once

#include <stun/SessionHandler.h>

#include <networking/IPAddressPool.h>
#include <networking/Messenger.h>
#include <networking/NetlinkClient.h>
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
  CommandCenter() : addrPool(new IPAddressPool("10.100.0.0", 16)) {}

  void serve(int port);
  void connect(std::string const& host, int port);

  std::unique_ptr<IPAddressPool> addrPool;

private:
  static int numClients;

  void handleAccept(TCPPipe&& client);

  TCPPipe commandServer;
  std::vector<SessionHandler> servers;
  std::unique_ptr<SessionHandler> client;
};
}
