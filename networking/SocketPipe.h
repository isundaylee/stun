#pragma once

#include <networking/Pipe.h>
#include <networking/SocketAddress.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>

namespace networking {

const int kSocketInboundQueueSize = 256;
const int kSocketOutboundQueueSize = 256;

const int kSocketPipeListenBacklog = 10;

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
    SocketAddress myAddr("0.0.0.0", port);
    int ret = ::bind(this->fd_, myAddr.asSocketAddress(), myAddr.getLength());
    checkUnixError(ret, "binding to SocketPipe's socket");

    // Getting the port that is actually used (in case that the given port is 0)
    SocketAddress myActualAddr;
    socklen_t myActualAddrLen = myActualAddr.getStorageLength();
    ret = getsockname(this->fd_, myActualAddr.asSocketAddress(),
                      &myActualAddrLen);
    checkUnixError(ret, "calling getsockname()");
    int actualPort = myActualAddr.getPort();

    // Listening
    if (type_ == TCP) {
      int ret = listen(this->fd_, kSocketPipeListenBacklog);
      checkUnixError(ret, "listening on a SocketPipe's socket");
    }

    LOG_T(this->name_) << "Started listening on port " << actualPort
                       << std::endl;

    bound_ = true;
    this->startActions();

    return actualPort;
  }

  void connect(std::string const& host, int port) {
    assertTrue(!connected_, "Connecting while already connected");
    assertTrue((type_ == SocketType::UDP) || !bound_,
               "Connecting while already bound");

    LOG_T(this->name_) << "Connecting to " << host << ":" << port << std::endl;

    SocketAddress peerAddr(host, port);
    int ret =
        ::connect(this->fd_, peerAddr.asSocketAddress(), peerAddr.getLength());
    checkUnixError(ret, "connecting in SocketPipe", EINPROGRESS);
    connected_ = true;
    this->peerAddr = peerAddr.getHost();
    this->peerPort = port;

    LOG_T(this->name_) << "Connected to " << host << ":" << port << std::endl;

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

  virtual void close() override {
    Pipe<P>::close();
    bound_ = false;
    connected_ = false;
  }

protected:
  SocketType type_;
  bool bound_;
  bool connected_;

  virtual bool read(P& packet) override {
    if (!connected_ && !bound_) {
      return false;
    }

    SocketAddress peerAddr;
    socklen_t peerAddrSize = peerAddr.getStorageLength();
    int ret = recvfrom(this->fd_, packet.data, packet.capacity, 0,
                       peerAddr.asSocketAddress(), &peerAddrSize);
    if (ret < 0 && errno == ECONNRESET) {
      // the socket is closed
      LOG_T(this->name_) << "Said goodbye (less peafully)" << std::endl;
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
      LOG_T(this->name_) << "Said goodbye" << std::endl;
      close();
      return false;
    }

    if (!connected_) {
      int ret = ::connect(this->fd_, peerAddr.asSocketAddress(),
                          peerAddr.getLength());
      checkUnixError(ret, "connecting in SocketPipe");
      connected_ = true;
      this->peerAddr = peerAddr.getHost();
      this->peerPort = peerAddr.getPort();
      LOG_T(this->name_) << "Received first UDP packet from " << this->peerAddr
                         << ":" << this->peerPort << std::endl;
    }

    return true;
  }

  virtual bool write(P const& packet) override {
    int ret = send(this->fd_, packet.data, packet.size, 0);

    // Just close the socket if the connection is refused (UDP) or reset (TCP)
    if (ret < 0 && (errno == ECONNRESET || errno == ECONNREFUSED)) {
      LOG_T(this->name_) << "Connection is refused/reset. Closing the socket."
                         << std::endl;
      close();
      return false;
    }

    if (!checkRetryableError(
            ret, "sending a " + std::string(type_ == TCP ? "TCP" : "UDP") +
                     " packet")) {
      return false;
    }
    return true;
  }

private:
  SocketPipe(SocketPipe const& copy) = delete;
  SocketPipe& operator=(SocketPipe const& copy) = delete;
};
}
