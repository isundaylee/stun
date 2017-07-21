#include "NetlinkClient.h"

#include <Util.h>

#include <unistd.h>
#include <string.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace stun {

const int kNetlinkMTU = 1000;

struct NetlinkRequest {
  struct nlmsghdr hdr;
  struct rtgenmsg gen;

  NetlinkRequest() {
    memset(this, 0, sizeof(NetlinkRequest));
  }
};

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

void NetlinkClient::sendRequest(NetlinkRequest& request) {
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

    if (len) {
      for (msg_ptr = (struct nlmsghdr*) replyBuffer;
           NLMSG_OK(msg_ptr, len);
           msg_ptr = NLMSG_NEXT(msg_ptr, len)) {
        switch (msg_ptr->nlmsg_type) {
        case 3:
          LOG() << "BREAKING!! " << std::endl;
          return;
        default:
          callback(msg_ptr);
          break;
        }
      }
    }
  }
}

void NetlinkClient::newLink(std::string const& deviceName) {
  LOG() << "Trying to turn up link " << deviceName << std::endl;

  NetlinkRequest req;

  req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
  req.hdr.nlmsg_type = RTM_GETLINK;
  req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  req.hdr.nlmsg_seq = 1;
  req.hdr.nlmsg_pid = getpid();
  req.gen.rtgen_family = AF_PACKET;

  sendRequest(req);
  waitForReply([](struct nlmsghdr *msg) {
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
          LOG() << "Found interface " << iface->ifi_index << " with name: " \
                << (char*) RTA_DATA(attr) << std::endl;
          break;
        default:
          break;
        }
      }

      break;
    default:
      LOG() << "UNEXPECTED!!!!! " << msg->nlmsg_type << std::endl;
    }
  });
}

}
