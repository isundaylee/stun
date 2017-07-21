#include "NetlinkClient.h"

#include <Util.h>

#include <unistd.h>
#include <string.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <stdexcept>

namespace stun {

const int kNetlinkMTU = 1000;

NetlinkClient::NetlinkClient() {
  socket_ = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  requestSeq_ = 0;

  memset(&localAddress_, 0, sizeof(localAddress_));
  localAddress_.nl_family = AF_NETLINK;
  localAddress_.nl_pid = getpid();
  localAddress_.nl_groups = 0;

  if (bind(socket_, (struct sockaddr *) &localAddress_, sizeof(localAddress_)) < 0) {
    throwUnixError("binding to NETLINK socket");
  }

  memset(&kernelAddress_, 0, sizeof(kernelAddress_));
  kernelAddress_.nl_family = AF_NETLINK;
}

NetlinkClient::~NetlinkClient() {
  close(socket_);
}

template <typename T>
void NetlinkClient::sendRequest(T& request) {
  struct msghdr rtnl_msg;
  struct iovec io;

  memset(&rtnl_msg, 0, sizeof(rtnl_msg));

  io.iov_base = &request;
  io.iov_len = request.hdr.nlmsg_len;
  rtnl_msg.msg_iov = &io;
  rtnl_msg.msg_iovlen = 1;
  rtnl_msg.msg_name = &kernelAddress_;
  rtnl_msg.msg_namelen = sizeof(kernelAddress_);

  sendmsg(socket_, (struct msghdr*) &rtnl_msg, 0);
}


void NetlinkClient::waitForReply(std::function<void (struct nlmsghdr *)> callback) {
  while (true) {
    int len;
    struct nlmsghdr *msg_ptr;
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
      throw std::runtime_error("NetlinkClient buffer size too small.");
    }

    if (len) {
      for (msg_ptr = (struct nlmsghdr*) replyBuffer;
           NLMSG_OK(msg_ptr, len);
           msg_ptr = NLMSG_NEXT(msg_ptr, len)) {
        struct nlmsgerr* err;

        switch (msg_ptr->nlmsg_type) {
        case NLMSG_ERROR:
          err = (struct nlmsgerr*) NLMSG_DATA(msg_ptr);
          if (err->error == 0) {
            // it's ACK, not an actual error
            return;
          } else {
            throw std::runtime_error("Got NLMSG_ERROR from netlink: " + std::string(strerror(-err->error)));
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

template <typename M>
struct NetlinkRequest {
  struct nlmsghdr hdr;
  M msg;
  char attrs[kNetlinkRequestAttrBufferSize];

  NetlinkRequest() {
    memset(this, 0, sizeof(*this));
  }

  void fillHeader(int flags) {
    hdr.nlmsg_len = NLMSG_LENGTH(sizeof(M));
    hdr.nlmsg_type = RTM_GETLINK;
    hdr.nlmsg_flags = NLM_F_REQUEST | flags;
    hdr.nlmsg_pid = getpid();
  }
};

typedef NetlinkRequest<struct rtgenmsg> NetlinkListLinkRequest;

int NetlinkClient::getInterfaceIndex(std::string const& deviceName) {
  NetlinkListLinkRequest req;

  req.fillHeader(NLM_F_DUMP);
  req.msg.rtgen_family = AF_PACKET;

  int index = -1;

  sendRequest(req);
  waitForReply([&index, deviceName](struct nlmsghdr *msg) {
    switch (msg->nlmsg_type) {
    case 16:
      struct ifinfomsg *iface;
      struct rtattr *attr;
      int len;

      iface = (ifinfomsg*) NLMSG_DATA(msg);
      len = msg->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));

      for (attr = IFLA_RTA(iface); RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
        switch (attr->rta_type) {
        case IFLA_IFNAME:
          if (std::string((char*) RTA_DATA(attr)) == deviceName) {
            index = iface->ifi_index;
          }
          break;
        default:
          break;
        }
      }

      break;
    default:
      throw std::runtime_error("Unexpected nlmsg_type " + std::to_string(msg->nlmsg_type));
    }
  });

  LOG() << "Interface index for device " << deviceName << " is " << index << std::endl;

  return index;
}

typedef NetlinkRequest<struct ifinfomsg> NetlinkChangeLinkRequest;

void NetlinkClient::newLink(std::string const& deviceName) {
  LOG() << "Trying to turn up link " << deviceName << std::endl;
  int interfaceIndex = getInterfaceIndex(deviceName);

  if (interfaceIndex < 0) {
    throw std::runtime_error("Cannot find device " + deviceName);
  }

  NetlinkChangeLinkRequest req;

  req.fillHeader(NLM_F_CREATE | NLM_F_ACK);
  req.msg.ifi_family = AF_UNSPEC;
  req.msg.ifi_index = interfaceIndex;
  req.msg.ifi_flags = IFF_UP;
  req.msg.ifi_change = IFF_UP;

  sendRequest(req);
  waitForReply([](struct nlmsghdr *msg) {});

  LOG() << "Successfully turned " << deviceName << " up" << std::endl;
}

}
