#include "networking/InterfaceConfig.h"

#include <common/Util.h>

#include <arpa/inet.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>

namespace networking {

InterfaceConfig::InterfaceConfig() {
  socket_ = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  requestSeq_ = 0;

  memset(&localAddress_, 0, sizeof(localAddress_));
  localAddress_.nl_family = AF_NETLINK;
  localAddress_.nl_pid = getpid();
  localAddress_.nl_groups = 0;

  int ret =
      bind(socket_, (struct sockaddr*)&localAddress_, sizeof(localAddress_));
  checkUnixError(ret, "binding to NETLINK socket");

  memset(&kernelAddress_, 0, sizeof(kernelAddress_));
  kernelAddress_.nl_family = AF_NETLINK;
}

InterfaceConfig::~InterfaceConfig() { close(socket_); }

template <typename T> void InterfaceConfig::sendRequest(T& request) {
  struct msghdr rtnl_msg;
  struct iovec io;

  memset(&rtnl_msg, 0, sizeof(rtnl_msg));

  io.iov_base = &request;
  io.iov_len = request.hdr.nlmsg_len;
  rtnl_msg.msg_iov = &io;
  rtnl_msg.msg_iovlen = 1;
  rtnl_msg.msg_name = &kernelAddress_;
  rtnl_msg.msg_namelen = sizeof(kernelAddress_);

  sendmsg(socket_, (struct msghdr*)&rtnl_msg, 0);
}

void InterfaceConfig::waitForReply(
    std::function<void(struct nlmsghdr*)> callback) {
  while (true) {
    int len;
    struct nlmsghdr* msg_ptr;
    struct msghdr rtnl_reply;
    struct iovec io_reply;

    memset(&io_reply, 0, sizeof(io_reply));
    memset(&rtnl_reply, 0, sizeof(rtnl_reply));

    io_reply.iov_base = replyBuffer;
    io_reply.iov_len = kNetlinkClientReplyBufferSize;
    rtnl_reply.msg_iov = &io_reply;
    rtnl_reply.msg_iovlen = 1;
    rtnl_reply.msg_name = &kernelAddress_;
    rtnl_reply.msg_namelen = sizeof(kernelAddress_);

    len = recvmsg(socket_, &rtnl_reply, 0);
    if (len == kNetlinkClientReplyBufferSize) {
      throw std::runtime_error("InterfaceConfig buffer size too small.");
    }

    if (len) {
      for (msg_ptr = (struct nlmsghdr*)replyBuffer; NLMSG_OK(msg_ptr, len);
           msg_ptr = NLMSG_NEXT(msg_ptr, len)) {
        struct nlmsgerr* err;

        switch (msg_ptr->nlmsg_type) {
        case NLMSG_ERROR:
          err = (struct nlmsgerr*)NLMSG_DATA(msg_ptr);
          if (err->error == 0) {
            // it's ACK, not an actual error
            return;
          } else if (-err->error == EEXIST) {
            LOG() << "Got EEXIST from netlink -- usually this is OK"
                  << std::endl;
          } else {
            throw std::runtime_error("Got NLMSG_ERROR from netlink: " +
                                     std::string(strerror(-err->error)));
          }
        case NLMSG_NOOP:
        case NLMSG_DONE:
          return;
        default:
          callback(msg_ptr);
          break;
        }
      }
    }
  }
}

template <typename M> struct NetlinkRequest {
  struct nlmsghdr hdr;
  M msg;
  char attrs[kNetlinkRequestAttrBufferSize];

  NetlinkRequest() { memset(this, 0, sizeof(*this)); }

  void fillHeader(int type, int flags) {
    hdr.nlmsg_len = NLMSG_LENGTH(sizeof(M));
    hdr.nlmsg_type = type;
    hdr.nlmsg_flags = NLM_F_REQUEST | flags;
    hdr.nlmsg_pid = getpid();
  }

  void addAttr(int type, int size, void* data) {
    struct rtattr* attr =
        (struct rtattr*)(((char*)this) + NLMSG_ALIGN(hdr.nlmsg_len));
    attr->rta_type = type;
    attr->rta_len = RTA_LENGTH(size);
    hdr.nlmsg_len = NLMSG_ALIGN(hdr.nlmsg_len) + RTA_LENGTH(size);
    memcpy(RTA_DATA(attr), data, size);
  }
};

typedef NetlinkRequest<struct rtgenmsg> NetlinkListLinkRequest;

int InterfaceConfig::getInterfaceIndex(std::string const& deviceName) {
  NetlinkListLinkRequest req;

  req.fillHeader(RTM_GETLINK, NLM_F_DUMP);
  req.msg.rtgen_family = AF_PACKET;

  int index = -1;

  sendRequest(req);
  waitForReply([&index, deviceName](struct nlmsghdr* msg) {
    switch (msg->nlmsg_type) {
    case 16:
      struct ifinfomsg* iface;
      struct rtattr* attr;
      int len;

      iface = (ifinfomsg*)NLMSG_DATA(msg);
      len = msg->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));

      for (attr = IFLA_RTA(iface); RTA_OK(attr, len);
           attr = RTA_NEXT(attr, len)) {
        switch (attr->rta_type) {
        case IFLA_IFNAME:
          if (std::string((char*)RTA_DATA(attr)) == deviceName) {
            index = iface->ifi_index;
          }
          break;
        default:
          break;
        }
      }

      break;
    default:
      throw std::runtime_error("Unexpected nlmsg_type " +
                               std::to_string(msg->nlmsg_type));
    }
  });

  if (index < 0) {
    throw std::runtime_error("Cannot find device " + deviceName);
  }

  return index;
}

typedef NetlinkRequest<struct ifinfomsg> NetlinkChangeLinkRequest;

void InterfaceConfig::newLink(std::string const& deviceName) {
  LOG() << "Turning up link " << deviceName << std::endl;
  int interfaceIndex = getInterfaceIndex(deviceName);

  NetlinkChangeLinkRequest req;

  req.fillHeader(RTM_NEWLINK, NLM_F_CREATE | NLM_F_ACK);
  req.msg.ifi_family = AF_UNSPEC;
  req.msg.ifi_index = interfaceIndex;
  req.msg.ifi_flags = IFF_UP;
  req.msg.ifi_change = IFF_UP;

  sendRequest(req);
  waitForReply([](struct nlmsghdr* msg) {});

  LOG() << "Successfully turned link up" << std::endl;
}

typedef NetlinkRequest<struct ifaddrmsg> NetlinkChangeAddressRequest;

void InterfaceConfig::setLinkAddress(std::string const& deviceName,
                                     std::string const& localAddress,
                                     std::string const& peerAddress) {
  LOG() << "Setting link " << deviceName << "'s address to " << localAddress
        << " -> " << peerAddress << std::endl;
  int interfaceIndex = getInterfaceIndex(deviceName);

  NetlinkChangeAddressRequest req;
  struct in_addr localAddr;
  struct in_addr peerAddr;
  inet_pton(AF_INET, localAddress.c_str(), &localAddr);
  inet_pton(AF_INET, peerAddress.c_str(), &peerAddr);

  req.fillHeader(RTM_NEWADDR, NLM_F_CREATE | NLM_F_ACK);
  req.msg.ifa_family = AF_INET;
  req.msg.ifa_prefixlen = 32;
  req.msg.ifa_index = interfaceIndex;
  req.addAttr(IFA_LOCAL, sizeof(localAddr), &localAddr);
  req.addAttr(IFA_ADDRESS, sizeof(peerAddr), &peerAddr);

  sendRequest(req);
  waitForReply([](struct nlmsghdr* msg) {});

  LOG() << "Successfully set link address" << std::endl;
}

typedef NetlinkRequest<struct rtmsg> NetlinkChangeRouteRequest;

void InterfaceConfig::newRoute(SubnetAddress const& destSubnet,
                               std::string const& gatewayAddr) {
  LOG() << "Adding a route to " << destSubnet.toString() << " via "
        << gatewayAddr << std::endl;

  NetlinkChangeRouteRequest req;
  req.fillHeader(RTM_NEWROUTE, NLM_F_CREATE | NLM_F_ACK);
  req.msg.rtm_family = AF_INET;
  req.msg.rtm_table = RT_TABLE_MAIN;
  req.msg.rtm_protocol = RTPROT_BOOT;
  req.msg.rtm_scope = RT_SCOPE_UNIVERSE;
  req.msg.rtm_type = RTN_UNICAST;
  req.msg.rtm_dst_len = destSubnet.prefixLen;

  struct in_addr gateway;
  struct in_addr dest;
  inet_pton(AF_INET, gatewayAddr.c_str(), &gateway);
  inet_pton(AF_INET, destSubnet.addr.c_str(), &dest);
  req.addAttr(RTA_GATEWAY, sizeof(gateway), &gateway);
  req.addAttr(RTA_DST, sizeof(dest), &dest);

  sendRequest(req);
  waitForReply([](struct nlmsghdr* msg) {});

  LOG() << "Successfully added a route" << std::endl;
}

typedef NetlinkRequest<struct rtmsg> NetlinkListRouteRequest;

std::string InterfaceConfig::getRoute(std::string const& destAddr) {
  NetlinkChangeRouteRequest req;
  req.fillHeader(RTM_GETROUTE, NLM_F_ACK);
  req.msg.rtm_family = AF_INET;
  req.msg.rtm_table = RT_TABLE_MAIN;
  req.msg.rtm_protocol = RTPROT_BOOT;
  req.msg.rtm_scope = RT_SCOPE_UNIVERSE;
  req.msg.rtm_type = RTN_UNICAST;
  req.msg.rtm_dst_len = 32;

  struct in_addr dest;
  inet_pton(AF_INET, destAddr.c_str(), &dest);
  req.addAttr(RTA_DST, sizeof(dest), &dest);

  std::string gateway = "";

  sendRequest(req);
  waitForReply([&gateway](struct nlmsghdr* msg) {
    switch (msg->nlmsg_type) {
    case 24:
      struct rtmsg* route;
      struct rtattr* attr;
      int len;

      route = (rtmsg*)NLMSG_DATA(msg);
      len = msg->nlmsg_len - NLMSG_LENGTH(sizeof(*route));

      for (attr = RTM_RTA(route); RTA_OK(attr, len);
           attr = RTA_NEXT(attr, len)) {
        switch (attr->rta_type) {
        case RTA_GATEWAY:
          char addressBuffer[32];
          inet_ntop(AF_INET, RTA_DATA(attr), addressBuffer,
                    sizeof(addressBuffer));
          gateway = std::string(addressBuffer);
          break;
        }
      }

      break;
    default:
      throw std::runtime_error("Unexpected nlmsg_type " +
                               std::to_string(msg->nlmsg_type));
    }
  });

  assertTrue(!gateway.empty(), "Error getting route to " + destAddr);
  return gateway;
}
}
