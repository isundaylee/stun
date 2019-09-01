#pragma once

#include <stun/ServerSessionHandler.h>

#include <event/Timer.h>
#include <networking/IPAddressPool.h>
#include <networking/TCPServer.h>

namespace stun {

using networking::IPAddressPool;
using networking::SubnetAddress;
using networking::TCPServer;

struct ServerConfig {
public:
  std::string configID;

  int port;
  SubnetAddress addressPool;
  bool encryption;
  std::string secret;
  size_t paddingTo;
  bool compression;
  event::Duration dataPipeRotationInterval;
  bool authentication;
  size_t mtu;
  std::map<std::string, size_t> quotaTable;
  std::map<std::string, IPAddress> staticHosts;
  std::vector<networking::IPAddress> dnsPushes;
};

class Server {
public:
  Server(event::EventLoop& loop, ServerConfig config);

  std::unique_ptr<IPAddressPool> addrPool;

private:
  event::EventLoop& loop_;

  ServerConfig config_;

  std::unique_ptr<TCPServer> server_;
  std::unique_ptr<event::Action> listener_;
  std::vector<std::unique_ptr<ServerSessionHandler>> sessionHandlers_;

  void doAccept();

  // FIXME: This should really be a inner class instead;
  friend ServerSessionHandler;
};
} // namespace stun
