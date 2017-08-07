#pragma once

#include <netinet/tcp.h>

#include <networking/Socket.h>

namespace networking {

class TCPSocket : public Socket {
public:
  TCPSocket() : Socket(TCP) {
    int flag = 1;
    int ret = setsockopt(fd_.fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    checkUnixError(ret, "setting TCP_NODELAY option for TCP socket");
  }

  TCPSocket(int fd, SocketAddress peerAddr) : Socket(TCP, fd, peerAddr) {}
};
}
