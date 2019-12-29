#pragma once

#include <networking/TCPSocket.h>

namespace networking {

class TCPServer : private TCPSocket {
public:
  using TCPSocket::bind;
  using TCPSocket::getPort;

  TCPServer(event::EventLoop& loop, NetworkType networkType)
      : TCPSocket(loop, networkType), loop_(loop) {}

  TCPSocket accept();
  event::Condition* canAccept() const;

private:
  event::EventLoop& loop_;
};
} // namespace networking
