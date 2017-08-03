#pragma once

#include <common/Util.h>

#include <networking/IPAddressPool.h>

#include <functional>
#include <string>

#if LINUX
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

namespace networking {

const size_t kNetlinkClientReplyBufferSize = (1U << 16);
const size_t kNetlinkRequestAttrBufferSize = (1U << 10);

struct RouteDestination {
public:
#if LINUX
  int interfaceIndex;
#elif OSX
  std::string interfaceName;
#endif
  std::string gatewayAddr;

#if LINUX
  RouteDestination(int interfaceIndex, std::string const& gatewayAddr)
      : interfaceIndex(interfaceIndex), gatewayAddr(gatewayAddr) {}
  explicit RouteDestination(std::string const& gatewayAddr)
      : interfaceIndex(-1), gatewayAddr(gatewayAddr) {}
#endif

#if OSX
  RouteDestination(std::string const& interfaceName,
                   std::string const& gatewayAddr)
      : interfaceName(interfaceName), gatewayAddr(gatewayAddr) {}
  explicit RouteDestination(std::string const& gatewayAddr)
      : interfaceName(""), gatewayAddr(gatewayAddr) {}
#endif
};

class InterfaceConfig {
public:
  InterfaceConfig();
  ~InterfaceConfig();

  void newLink(std::string const& deviceName, unsigned int mtu);
  void setLinkAddress(std::string const& deviceName,
                      std::string const& localAddress,
                      std::string const& peerAddress);

  void newRoute(SubnetAddress const& destSubnet,
                RouteDestination const& routeDest);
  RouteDestination getRoute(std::string const& destAddr);

private:
  InterfaceConfig(InterfaceConfig const&) = delete;
  InterfaceConfig& operator=(InterfaceConfig const&) = delete;

#if LINUX
  template <typename R> void sendRequest(R& req);
  void waitForReply(std::function<void(struct nlmsghdr*)> callback);
  int getInterfaceIndex(std::string const& deviceName);

  int socket_;
  int requestSeq_;
  char replyBuffer[kNetlinkClientReplyBufferSize];
  struct sockaddr_nl localAddress_;
  struct sockaddr_nl kernelAddress_;
#endif
};
}
