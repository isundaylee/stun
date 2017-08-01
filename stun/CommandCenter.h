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
  CommandCenter();

  std::unique_ptr<IPAddressPool> addrPool;

  void serve(int port);
  void connect(std::string const& host, int port);

  event::Condition* didDisconnect() const;

private:
  CommandCenter(CommandCenter const& copy) = delete;
  CommandCenter& operator=(CommandCenter const& copy) = delete;

  CommandCenter(CommandCenter&& move) = delete;
  CommandCenter& operator=(CommandCenter&& move) = delete;

  static int numClients;

  void handleAccept(TCPPipe&& client);

  std::unique_ptr<TCPPipe> commandServer_;
  std::vector<std::unique_ptr<SessionHandler>> servers_;
  std::unique_ptr<SessionHandler> client_;
  std::unique_ptr<event::BaseCondition> didDisconnect_;
};
}
