#include "UDPPipe.h"

#include <Util.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <string>

namespace stun {

UDPPipe::UDPPipe() :
    Pipe(kUDPInboundQueueSize, kUDPOutboundQueueSize),
    connected_(false) {
  int fd_ = socket(PF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    throwUnixError("creating UDPPipe's socket");
  }

  if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK) < 0) {
    throwUnixError("setting O_NONBLOCK for UDPPipe");
  }

  name = "UDP carrier";
  shouldOutputStats = true;
  setFd(fd_);
}

struct addrinfo* UDPPipe::getAddr(std::string const& host, int port) {
  struct addrinfo hints;
  struct addrinfo *addr;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  int err;
  if ((err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addr)) != 0) {
    throwGetAddrInfoError(err);
  }

  return addr;
}

void UDPPipe::bind(int port) {
  struct addrinfo* myAddr = getAddr("0.0.0.0", port);

  int ret = ::bind(fd_, myAddr->ai_addr, myAddr->ai_addrlen);
  if (ret < 0) {
    throwUnixError("binding to UDPPipe's socket");
  }

  freeaddrinfo(myAddr);

  LOG() << "UDPPipe started listening on port " << port << std::endl;
}

void UDPPipe::connect(std::string const& host, int port) {
  LOG() << "UDPPipe connecting to " << host << ":" << port << std::endl;

  struct addrinfo* peerAddr = getAddr(host, port);

  if (connected_) {
    throw std::runtime_error("Connecting while already connected");
  }

  int ret = ::connect(fd_, peerAddr->ai_addr, peerAddr->ai_addrlen);
  if (ret < 0) {
    throwUnixError("connecting in UDPPipe");
  }
  connected_ = true;

  freeaddrinfo(peerAddr);

  LOG() << "UDPPipe connected to " << host << ":" << port << std::endl;
}

bool UDPPipe::read(UDPPacket& packet) {
  struct sockaddr_storage peerAddr;
  socklen_t peerAddrSize = sizeof(peerAddr);

  int ret = recvfrom(fd_, packet.buffer, kUDPPacketBufferSize, 0, (sockaddr *) &peerAddr, &peerAddrSize);
  if (ret < 0) {
    if (errno != EAGAIN) {
      throwUnixError("receiving a UDP packet");
    }
    return false;
  }
  packet.size = ret;

  if (!connected_) {
    int ret = ::connect(fd_, (sockaddr*) &peerAddr, peerAddrSize);
    if (ret < 0) {
      throwUnixError("connecting in UDPPipe");
    }
    connected_ = true;
  }

  return true;
}

bool UDPPipe::write(UDPPacket const& packet) {
  int ret = send(fd_, packet.buffer, packet.size, 0);
  if (ret < 0) {
    if (errno != EAGAIN) {
      throwUnixError("sending a UDP packet");
    }
    return false;
  }

  return true;
}

UDPPipe::~UDPPipe() {
  close(fd_);
}

}
