#include "networking/TCPPipe.h"

namespace networking {

bool TCPPipe::read(TCPPacket& packet) {
  if (!bound_) {
    // We are a normal client-server pipe
    return SocketPipe<TCPPacket>::read(packet);
  }

  struct sockaddr_storage clientAddr;
  socklen_t clientAddrLen = sizeof(clientAddr);
  int client = accept(fd_, (struct sockaddr*) &clientAddr, &clientAddrLen);
  if (!checkRetryableError(client, "accepting a TCP client")) {
    return false;
  }

  TCPPipe clientPipe(client);
  onAccept(std::move(clientPipe));

  return false;
}

}
