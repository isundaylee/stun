#pragma once

#include <netinet/tcp.h>

#include <networking/Socket.h>

namespace networking {

class TCPSocket : public Socket {
public:
  TCPSocket(event::EventLoop& loop, std::string logPrefix,
            NetworkType networkType)
      : Socket(loop, logPrefix, networkType, TCP) {
    int flag = 1;
    int ret = setsockopt(fd_.fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    checkUnixError(ret, "setting TCP_NODELAY option for TCP socket");
  }

  TCPSocket(event::EventLoop& loop, std::string logPrefix,
            NetworkType networkType, int fd, SocketAddress peerAddr)
      : Socket(loop, logPrefix, networkType, TCP, fd, peerAddr) {}

  TCPSocket(event::EventLoop& loop, NetworkType networkType)
      : TCPSocket{loop, "", networkType} {}
  TCPSocket(event::EventLoop& loop, NetworkType networkType, int fd,
            SocketAddress peerAddr)
      : TCPSocket{loop, "", networkType, fd, peerAddr} {}
};
} // namespace networking
