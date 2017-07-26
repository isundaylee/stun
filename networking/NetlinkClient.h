#pragma once

#include <functional>
#include <string>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace networking {

const size_t kNetlinkClientReplyBufferSize = (1U << 16);
const size_t kNetlinkRequestAttrBufferSize = (1U << 10);

class NetlinkClient {
public:
  NetlinkClient();
  ~NetlinkClient();

  void newLink(std::string const& deviceName);
  void setLinkAddress(std::string const& deviceName,
                      std::string const& localAddress,
                      std::string const& peerAddress);

private:
  NetlinkClient(NetlinkClient const&) = delete;
  NetlinkClient& operator=(NetlinkClient const&) = delete;

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
