#pragma once

#include <stun/ServerSessionHandler.h>

#include <event/Timer.h>
#include <networking/IPAddressPool.h>
#include <networking/TCPServer.h>

namespace stun {

using networking::SubnetAddress;
using networking::TCPServer;
using networking::IPAddressPool;

class Server {
public:
  Server(ServerConfig config);

  std::unique_ptr<IPAddressPool> addrPool;

private:
  ServerConfig config_;

  std::unique_ptr<TCPServer> server_;
  std::unique_ptr<event::Action> listener_;
  std::vector<std::unique_ptr<ServerSessionHandler>> sessionHandlers_;

  void doAccept();
};
}
