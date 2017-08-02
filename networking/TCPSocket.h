#pragma once

#include <networking/Socket.h>

namespace networking {

class TCPSocket : public Socket {
public:
  TCPSocket() : Socket(TCP) {}
  TCPSocket(int fd, SocketAddress peerAddr) : Socket(TCP, fd, peerAddr) {}
};
}
