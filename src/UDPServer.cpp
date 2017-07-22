#include "UDPServer.h"

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

UDPServer::UDPServer() :
    inboundQ(kUDPInboundQueueSize),
    outboundQ(kUDPOutboundQueueSize),
    connected_(false) {
  socket_ = socket(PF_INET, SOCK_DGRAM, 0);
  if (socket_ < 0) {
    throwUnixError("creating UDPServer's socket");
  }

  if (fcntl(socket_, F_SETFL, fcntl(socket_, F_GETFL, 0) | O_NONBLOCK) < 0) {
    throwUnixError("setting O_NONBLOCK for UDPServer");
  }

  receiveWatcher_.set<UDPServer, &UDPServer::doReceive>(this);
  receiveWatcher_.set(socket_, ev::READ);
  receiveWatcher_.start();

  sendWatcher_.set<UDPServer, &UDPServer::doSend>(this);
  sendWatcher_.set(socket_, ev::WRITE);
  sendWatcher_.start();
}

struct addrinfo* UDPServer::getAddr(std::string const& host, int port) {
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

void UDPServer::bind(int port) {
  struct addrinfo* myAddr = getAddr("0.0.0.0", port);

  int ret = ::bind(socket_, myAddr->ai_addr, myAddr->ai_addrlen);
  if (ret < 0) {
    throwUnixError("binding to UDPServer's socket");
  }

  freeaddrinfo(myAddr);

  LOG() << "UDPServer started listening on port " << port << std::endl;
}

void UDPServer::connect(std::string const& host, int port) {
  LOG() << "UDPServer connecting to " << host << ":" << port << std::endl;

  struct addrinfo* peerAddr = getAddr(host, port);

  if (connected_) {
    throw std::runtime_error("Connecting while already connected");
  }

  int ret = ::connect(socket_, peerAddr->ai_addr, peerAddr->ai_addrlen);
  if (ret < 0) {
    throwUnixError("connecting in UDPServer");
  }
  connected_ = true;

  freeaddrinfo(peerAddr);

  LOG() << "UDPServer connected to " << host << ":" << port << std::endl;
}

void UDPServer::doReceive(ev::io& watcher, int events) {
  if (events & EV_ERROR) {
    throwUnixError("UDPServer doReceive()");
  }

  if (inboundQ.full()) {
    return;
  }

  struct sockaddr_storage peerAddr;
  socklen_t peerAddrSize = sizeof(peerAddr);

  UDPPacket packet;
  int ret = recvfrom(socket_, packet.data, kUDPPacketBufferSize, 0, (sockaddr *) &peerAddr, &peerAddrSize);
  if (ret < 0) {
    if (errno != EAGAIN) {
      throwUnixError("receiving a UDP packet");
    }
    return;
  }
  packet.size = ret;

  LOG() << "Receiving UDP <== " << packet.size << " bytes" << std::endl;

  if (!connected_) {
    int ret = ::connect(socket_, (sockaddr*) &peerAddr, peerAddrSize);
    if (ret < 0) {
      throwUnixError("connecting in UDPServer");
    }
    connected_ = true;
  }

  inboundQ.push(packet);
}

void UDPServer::doSend(ev::io& watcher, int events) {
  if (events & EV_ERROR) {
    throwUnixError("UDPServer doSend()");
  }

  if (outboundQ.empty()) {
    return;
  }

  UDPPacket packet = outboundQ.pop();
  int ret = send(socket_, packet.data, packet.size, 0);
  if (ret < 0 && errno != EAGAIN) {
    throwUnixError("sending a UDP packet");
  }

  LOG() << "Sending " << packet.size << " bytes ==> UDP" << std::endl;
}

UDPServer::~UDPServer() {
  close(socket_);
}

}
