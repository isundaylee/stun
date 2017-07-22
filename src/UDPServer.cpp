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

UDPServer::UDPServer(int port) :
    inboundQ(kUDPInboundQueueSize),
    outboundQ(kUDPOutboundQueueSize),
    connected_(false) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int err;
  if ((err = getaddrinfo("0.0.0.0", std::to_string(port).c_str(), &hints, &myAddr_)) != 0) {
    throwGetAddrInfoError(err);
  }

  socket_ = socket(myAddr_->ai_family, myAddr_->ai_socktype, myAddr_->ai_protocol);
  if (socket_ < 0) {
    throwUnixError("creating UDPServer's socket");
  }
}

void UDPServer::bind() {
  int ret = ::bind(socket_, myAddr_->ai_addr, myAddr_->ai_addrlen);
  if (ret < 0) {
    throwUnixError("binding to UDPServer's socket");
  }

  fcntl(socket_, F_SETFL, fcntl(socket_, F_GETFL, 0) | O_NONBLOCK);

  receiveWatcher_.set<UDPServer, &UDPServer::doReceive>(this);
  receiveWatcher_.set(socket_, ev::READ);
  receiveWatcher_.start();

  sendWatcher_.set<UDPServer, &UDPServer::doSend>(this);
  sendWatcher_.set(socket_, ev::WRITE);
  sendWatcher_.start();
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

  if (!connected_) {
    int ret = connect(socket_, (sockaddr*) &peerAddr, peerAddrSize);
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
}

UDPServer::~UDPServer() {
  close(socket_);
  freeaddrinfo(myAddr_);
}

}
