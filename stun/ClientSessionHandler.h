#pragma once

#include <stun/Dispatcher.h>

#include <event/Promise.h>
#include <event/Timer.h>
#include <networking/IPAddressPool.h>
#include <networking/Messenger.h>
#include <networking/TCPSocket.h>

namespace stun {

using namespace networking;

struct ClientTunnelConfig {
public:
  IPAddress myTunnelAddr;
  IPAddress peerTunnelAddr;
  SubnetAddress serverSubnetAddr;
  size_t mtu;
  std::vector<SubnetAddress> subnetsToForward;
  std::vector<SubnetAddress> subnetsToExclude;
  std::vector<IPAddress> dnsPushes;
  bool acceptDNSPushes;
};

struct ClientConfig {
public:
  SocketAddress serverAddr;

  bool encryption;
  std::string secret;
  size_t paddingTo;
  event::Duration dataPipeRotationInterval;
  std::string user;
  size_t mtu;
  bool acceptDNSPushes;

  std::vector<SubnetAddress> subnetsToForward;
  std::vector<SubnetAddress> subnetsToExclude;
};

class ClientSessionHandler {
public:
  using TunnelFactory = std::function<std::shared_ptr<
      event::Promise<std::unique_ptr<networking::Tunnel>>>(ClientTunnelConfig)>;

  ClientSessionHandler(event::EventLoop& loop, ClientConfig config,
                       std::unique_ptr<TCPSocket> commandPipe,
                       TunnelFactory tunnelFactory);

  event::Condition* didEnd() const;

private:
  event::EventLoop& loop_;

  ClientConfig config_;
  TunnelFactory tunnelFactory_;

  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<Dispatcher> dispatcher_;

  std::unique_ptr<event::BaseCondition> didEnd_;

  void attachHandlers();
};
} // namespace stun
