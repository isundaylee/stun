#pragma once

#include <common/Configerator.h>

#include <stun/SessionHandler.h>

#include <networking/IPAddressPool.h>
#include <networking/InterfaceConfig.h>
#include <networking/Messenger.h>
#include <networking/TCPServer.h>
#include <networking/TCPSocket.h>
#include <networking/Tunnel.h>

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

  void doAccept();

  std::unique_ptr<TCPServer> server_;
  std::unique_ptr<event::Action> listener_;
  std::vector<std::unique_ptr<SessionHandler>> serverHandlers_;
  std::unique_ptr<SessionHandler> clientHandler_;
  std::unique_ptr<event::BaseCondition> didDisconnect_;
};
}
