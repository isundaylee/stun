#pragma once

#include <Pipe.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

namespace stun {

const int kSocketInboundQueueSize = 32;
const int kSocketOutboundQueueSize = 32;

struct SocketPacket: PipePacket {};

enum SocketType {
  TCP,
  UDP
};

template <typename P>
class SocketPipe: public Pipe<P> {
public:
  SocketPipe(SocketType type) :
      Pipe<P>(kSocketInboundQueueSize, kSocketOutboundQueueSize),
      type_(type),
      connected_(false) {
    int fd_ = socket(PF_INET, type_ == TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (fd_ < 0) {
      throwUnixError("creating SocketPipe's socket");
    }

    if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK) < 0) {
      throwUnixError("setting O_NONBLOCK for SocketPipe");
    }

    this->name = "UDP carrier";
    this->shouldOutputStats = true;
    this->setFd(fd_);
  }

  ~SocketPipe() {
    close(this->fd_);
  }

  void bind(int port) {
    struct addrinfo* myAddr = getAddr("0.0.0.0", port);

    int ret = ::bind(this->fd_, myAddr->ai_addr, myAddr->ai_addrlen);
    if (ret < 0) {
      throwUnixError("binding to SocketPipe's socket");
    }

    freeaddrinfo(myAddr);

    LOG() << "SocketPipe started listening on port " << port << std::endl;
  }

  void connect(std::string const& host, int port) {
    LOG() << "SocketPipe connecting to " << host << ":" << port << std::endl;

    struct addrinfo* peerAddr = getAddr(host, port);

    if (connected_) {
      throw std::runtime_error("Connecting while already connected");
    }

    int ret = ::connect(this->fd_, peerAddr->ai_addr, peerAddr->ai_addrlen);
    if (ret < 0) {
      throwUnixError("connecting in SocketPipe");
    }
    connected_ = true;

    freeaddrinfo(peerAddr);

    LOG() << "SocketPipe connected to " << host << ":" << port << std::endl;
  }

private:
  SocketPipe(SocketPipe const& copy) = delete;
  SocketPipe& operator=(SocketPipe const& copy) = delete;

  struct addrinfo* getAddr(std::string const& host, int port) {
    struct addrinfo hints;
    struct addrinfo *addr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = type_ == TCP ? SOCK_STREAM : SOCK_DGRAM;

    int err;
    if ((err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addr)) != 0) {
      throwGetAddrInfoError(err);
    }

    return addr;
  }

  bool read(P& packet) override {
    struct sockaddr_storage peerAddr;
    socklen_t peerAddrSize = sizeof(peerAddr);

    int ret = recvfrom(this->fd_, packet.buffer, kPipePacketBufferSize, 0, (sockaddr *) &peerAddr, &peerAddrSize);
    if (ret < 0) {
      if (errno != EAGAIN) {
        throwUnixError("receiving a UDP packet");
      }
      return false;
    }
    packet.size = ret;

    if (!connected_) {
      int ret = ::connect(this->fd_, (sockaddr*) &peerAddr, peerAddrSize);
      if (ret < 0) {
        throwUnixError("connecting in SocketPipe");
      }
      connected_ = true;
    }

    return true;
  }

  bool write(P const& packet) override {
    int ret = send(this->fd_, packet.buffer, packet.size, 0);
    if (ret < 0) {
      if (errno != EAGAIN) {
        throwUnixError("sending a UDP packet");
      }
      return false;
    }

    return true;
  }

  SocketType type_;
  bool connected_;
};

}
