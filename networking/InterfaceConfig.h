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
  IPAddress gatewayAddr;

#if LINUX
  RouteDestination(int interfaceIndex, IPAddress const& gatewayAddr)
      : interfaceIndex(interfaceIndex), gatewayAddr(gatewayAddr) {}
  explicit RouteDestination(IPAddress const& gatewayAddr)
      : interfaceIndex(-1), gatewayAddr(gatewayAddr) {}
#endif

#if OSX
  RouteDestination(std::string const& interfaceName,
                   IPAddress const& gatewayAddr)
      : interfaceName(interfaceName), gatewayAddr(gatewayAddr) {}
  explicit RouteDestination(IPAddress const& gatewayAddr)
      : interfaceName(""), gatewayAddr(gatewayAddr) {}
#endif

#if IOS
  explicit RouteDestination(IPAddress const& gatewayAddr)
      : gatewayAddr(gatewayAddr) {}
#endif
};

struct Route {
public:
  SubnetAddress subnet;
  RouteDestination dest;
};

class InterfaceConfig {
public:
  InterfaceConfig();
  ~InterfaceConfig();

  InterfaceConfig(InterfaceConfig&& move) = default;
  InterfaceConfig& operator=(InterfaceConfig&& move) = default;

  static void newLink(std::string const& deviceName, unsigned int mtu);
  static void setLinkAddress(std::string const& deviceName,
                             IPAddress const& localAddress,
                             IPAddress const& peerAddress);

  static void newRoute(Route const& route);
  static RouteDestination getRoute(IPAddress const& destAddr);

private:
  InterfaceConfig(InterfaceConfig const&) = delete;
  InterfaceConfig& operator=(InterfaceConfig const&) = delete;

  static InterfaceConfig& getInstance();

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
