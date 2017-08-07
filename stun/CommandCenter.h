#pragma once

#include <common/Configerator.h>

#include <stun/Server.h>
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

struct ClientConfig {
public:
  SocketAddress serverAddr;

  bool encryption;
  std::string secret;
  size_t paddingTo;
  event::Duration dataPipeRotationInterval;

  std::vector<SubnetAddress> subnetsToForward;
  std::vector<SubnetAddress> subnetsToExclude;
};

class SessionHandler;

class CommandCenter {
public:
  CommandCenter();

  void serve(ServerConfig config);
  void connect(ClientConfig config);

  event::Condition* didDisconnect() const;

private:
  std::unique_ptr<class Server> server_;

  std::unique_ptr<SessionHandler> clientHandler_;
  std::unique_ptr<event::BaseCondition> didDisconnect_;

private:
  CommandCenter(CommandCenter const& copy) = delete;
  CommandCenter& operator=(CommandCenter const& copy) = delete;

  CommandCenter(CommandCenter&& move) = delete;
  CommandCenter& operator=(CommandCenter&& move) = delete;
};
}
