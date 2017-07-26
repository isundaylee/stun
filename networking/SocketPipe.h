#pragma once

#include <networking/Pipe.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unistd.h>

namespace networking {

const int kSocketInboundQueueSize = 32;
const int kSocketOutboundQueueSize = 32;

const int kSocketPipeListenBacklog = 10;

struct SocketPacket : PipePacket {};

enum SocketType { TCP, UDP };

template <typename P> class SocketPipe : public Pipe<P> {
public:
  SocketPipe(SocketType type)
      : Pipe<P>(kSocketInboundQueueSize, kSocketOutboundQueueSize), type_(type),
        bound_(false), connected_(false) {}

  SocketPipe(SocketPipe&& move)
      : Pipe<P>(std::move(move)), peerAddr(move.peerAddr),
        peerPort(move.peerPort), type_(move.type_), bound_(move.bound_),
        connected_(move.connected_) {}

  std::string peerAddr;
  int peerPort;

  int bind(int port) {
    assertTrue(!bound_, "Calling bind() on a bound SocketPipe");
    assertTrue(!connected_, "Calling bind() on a connected SocketPipe");

    // Binding
    struct addrinfo* myAddr = getAddr("0.0.0.0", port);
    int ret = ::bind(this->fd_, myAddr->ai_addr, myAddr->ai_addrlen);
    checkUnixError(ret, "binding to SocketPipe's socket");
    freeaddrinfo(myAddr);

    // Getting the port that is actually used (in case that the given port is 0)
    struct sockaddr_storage myActualAddr;
    socklen_t myActualAddrLen = sizeof(myActualAddr);
    ret = getsockname(this->fd_, (struct sockaddr*)&myActualAddr,
                      &myActualAddrLen);
    checkUnixError(ret, "calling getsockname()");
    int actualPort = ntohs(((struct sockaddr_in*)&myActualAddr)->sin_port);

    // Listening
    if (type_ == TCP) {
      int ret = listen(this->fd_, kSocketPipeListenBacklog);
      checkUnixError(ret, "listening on a SocketPipe's socket");
    }

    LOG() << "SocketPipe started listening on port " << actualPort << std::endl;

    bound_ = true;
    this->startActions();

    return actualPort;
  }

  void connect(std::string const& host, int port) {
    assertTrue(!connected_, "Connecting while already connected");
    assertTrue((type_ == SocketType::UDP) || !bound_,
               "Connecting while already bound");

    LOG() << "SocketPipe connecting to " << host << ":" << port << std::endl;

    struct addrinfo* peerAddr = getAddr(host, port);

    int ret = ::connect(this->fd_, peerAddr->ai_addr, peerAddr->ai_addrlen);
    checkUnixError(ret, "connecting in SocketPipe", EINPROGRESS);
    connected_ = true;
    this->peerAddr = host;
    this->peerPort = port;

    freeaddrinfo(peerAddr);

    LOG() << "SocketPipe connected to " << host << ":" << port << std::endl;

    this->startActions();
  }

  virtual void open() override {
    assertTrue(this->fd_ == 0, "trying to open an already open SocketPipe");

    int fd_ = socket(PF_INET, type_ == TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
    checkUnixError(fd_, "creating SocketPipe's socket");

    int ret = fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
    checkUnixError(ret, "setting O_NONBLOCK for SocketPipe");

    int yes = 1;
    ret = setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    checkUnixError(ret, "setting SO_REUSEADDR for SocketPipe");

    this->fd_ = fd_;
  }

protected:
  SocketType type_;
  bool bound_;
  bool connected_;

  virtual bool read(P& packet) override {
    if (!connected_ && !bound_) {
      return false;
    }

    struct sockaddr_storage peerAddr;
    socklen_t peerAddrSize = sizeof(peerAddr);

    int ret = recvfrom(this->fd_, packet.buffer, kPipePacketBufferSize, 0,
                       (sockaddr*)&peerAddr, &peerAddrSize);
    if (ret < 0 && errno == ECONNRESET) {
      // the socket is closed
      LOG() << "Goodbye, " << this->name << " (less peacfully)!" << std::endl;
      close();
      return false;
    }

    if (!checkRetryableError(
            ret, "receiving a " + std::string(type_ == TCP ? "TCP" : "UDP") +
                     " packet")) {
      return false;
    }
    packet.size = ret;

    if (ret == 0) {
      // the socket is closed
      LOG() << "Goodbye, " << this->name << "!" << std::endl;
      close();
      return false;
    }

    if (!connected_) {
      int ret = ::connect(this->fd_, (sockaddr*)&peerAddr, peerAddrSize);
      checkUnixError(ret, "connecting in SocketPipe");
      connected_ = true;
      this->peerAddr = getAddr((sockaddr*)&peerAddr);
      this->peerPort = getPort((sockaddr*)&peerAddr);
      LOG() << this->name << " received first UDP packet from "
            << this->peerAddr << ":" << this->peerPort << std::endl;
    }

    return true;
  }

  virtual bool write(P const& packet) override {
    int ret = send(this->fd_, packet.buffer, packet.size, 0);
    if (!checkRetryableError(
            ret, "sending a UDP " + std::string(type_ == TCP ? "TCP" : "UDP") +
                     " packet")) {
      return false;
    }
    return true;
  }

  virtual void close() override {
    Pipe<P>::close();
    bound_ = false;
    connected_ = false;
  }

  std::string getAddr(struct sockaddr* addrInfo) {
    assertTrue(addrInfo->sa_family == AF_INET,
               "Unsupported sa_family: " + std::to_string(addrInfo->sa_family));
    return std::string(inet_ntoa(((sockaddr_in*)addrInfo)->sin_addr));
  }

  int getPort(struct sockaddr* addrInfo) {
    assertTrue(addrInfo->sa_family == AF_INET,
               "Unsupported sa_family: " + std::to_string(addrInfo->sa_family));
    return ((sockaddr_in*)addrInfo)->sin_port;
  }

private:
  SocketPipe(SocketPipe const& copy) = delete;
  SocketPipe& operator=(SocketPipe const& copy) = delete;

  struct addrinfo* getAddr(std::string const& host, int port) {
    struct addrinfo hints;
    struct addrinfo* addr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = type_ == TCP ? SOCK_STREAM : SOCK_DGRAM;

    int err;
    if ((err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints,
                           &addr)) != 0) {
      throwGetAddrInfoError(err);
    }

    return addr;
  }
};
}
