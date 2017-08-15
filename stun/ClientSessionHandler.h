#pragma once

#include <stun/Dispatcher.h>

#include <event/Timer.h>
#include <networking/IPAddressPool.h>
#include <networking/Messenger.h>
#include <networking/TCPSocket.h>

namespace stun {

using namespace networking;

struct ClientConfig {
public:
  SocketAddress serverAddr;

  bool encryption;
  std::string secret;
  size_t paddingTo;
  event::Duration dataPipeRotationInterval;
  std::string user;

  std::vector<SubnetAddress> subnetsToForward;
  std::vector<SubnetAddress> subnetsToExclude;
};

class ClientSessionHandler {
public:
  ClientSessionHandler(ClientConfig config,
                       std::unique_ptr<TCPSocket> commandPipe);

  event::Condition* didEnd() const;

private:
  ClientConfig config_;

  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<Dispatcher> dispatcher_;

  std::unique_ptr<event::BaseCondition> didEnd_;

  void attachHandlers();
  Tunnel createTunnel(IPAddress const& myAddr, IPAddress const& peerAddr,
                      SubnetAddress const& serverSubnetAddr);
};
}
