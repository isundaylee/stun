#include "networking/TCPServer.h"

namespace networking {

TCPSocket TCPServer::accept() {
  assertTrue(bound_, "Socket::accept() can only be called on a bound socket.");

  SocketAddress peerAddr;
  socklen_t peerAddrLen = peerAddr.getStorageLength();
  int client = ::accept(fd_.fd, peerAddr.asSocketAddress(), &peerAddrLen);
  if (client < 0) {
    throwUnixError("in Socket::accept()");
  }

  return TCPSocket(loop_, this->networkType_, client, peerAddr);
}

event::Condition* TCPServer::canAccept() const { return canRead(); }
} // namespace networking
