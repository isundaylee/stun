#pragma once

#include <string>
#include <functional>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace stun {

const size_t kNetlinkClientReplyBufferSize = 2048;

class NetlinkClient {
public:
  NetlinkClient();
  ~NetlinkClient();

  void newLink(std::string const& deviceName);

private:
  NetlinkClient(NetlinkClient const&) = delete;
  NetlinkClient& operator=(NetlinkClient const&) = delete;

  template <typename R>
  void sendRequest(R& req);
  void waitForReply(std::function<void (struct nlmsghdr *)> callback);
  int getInterfaceIndex(std::string const& deviceName);

  int socket_;
  int requestSeq_;
  char replyBuffer[kNetlinkClientReplyBufferSize];
  struct sockaddr_nl localAddress_;
  struct sockaddr_nl kernelAddress_;
};

}
