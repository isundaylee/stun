#pragma once

#include <networking/IPAddressPool.h>

#include <functional>
#include <string>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace networking {

const size_t kNetlinkClientReplyBufferSize = (1U << 16);
const size_t kNetlinkRequestAttrBufferSize = (1U << 10);

struct RouteDestination {
public:
  int interfaceIndex;
  std::string const& gatewayAddr;

  RouteDestination(int interfaceIndex, std::string const& gatewayAddr)
      : interfaceIndex(interfaceIndex), gatewayAddr(gatewayAddr) {}
  explicit RouteDestination(std::string const& gatewayAddr)
      : interfaceIndex(-1), gatewayAddr(gatewayAddr) {}
};

class InterfaceConfig {
public:
  InterfaceConfig();
  ~InterfaceConfig();

  void newLink(std::string const& deviceName);
  void setLinkAddress(std::string const& deviceName,
                      std::string const& localAddress,
                      std::string const& peerAddress);

  void newRoute(SubnetAddress const& destSubnet,
                RouteDestination const& routeDest);
  RouteDestination getRoute(std::string const& destAddr);

private:
  InterfaceConfig(InterfaceConfig const&) = delete;
  InterfaceConfig& operator=(InterfaceConfig const&) = delete;

  template <typename R> void sendRequest(R& req);
  void waitForReply(std::function<void(struct nlmsghdr*)> callback);
  int getInterfaceIndex(std::string const& deviceName);

  int socket_;
  int requestSeq_;
  char replyBuffer[kNetlinkClientReplyBufferSize];
  struct sockaddr_nl localAddress_;
  struct sockaddr_nl kernelAddress_;
};
}
