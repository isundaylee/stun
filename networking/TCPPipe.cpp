#include "networking/TCPPipe.h"

namespace networking {

bool TCPPipe::read(TCPPacket& packet) {
  if (!bound_) {
    // We are a normal client-server pipe
    return SocketPipe<TCPPacket>::read(packet);
  }

  SocketAddress clientAddr;
  socklen_t clientAddrLen = clientAddr.getStorageLength();
  int client = accept(fd_, clientAddr.asSocketAddress(), &clientAddrLen);
  if (!checkRetryableError(client, "accepting a TCP client")) {
    return false;
  }

  std::string peerAddr = clientAddr.getHost();
  int peerPort = clientAddr.getPort();

  LOG_T(name_) << "Accepted incoming client from " << peerAddr << ":"
               << peerPort << std::endl;

  TCPPipe clientPipe(client, peerAddr, peerPort);
  onAccept(std::move(clientPipe));

  return false;
}
}
