#include "networking/Socket.h"

#include <common/Util.h>
#include <event/IOCondition.h>

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <mutex>
#include <string>

namespace networking {

const int kSocketListenBacklog = 10;

Socket::Socket(event::EventLoop& loop, NetworkType networkType, SocketType type)
    : loop_(loop), networkType_(networkType), type_(type), bound_(false),
      connected_(false) {
  signal(SIGPIPE, SIG_IGN);
  LOG_V("Socket") << "Disabled SIGPIPE handling." << std::endl;

  int fd = socket(networkType == NetworkType::IPv4 ? PF_INET : PF_INET6,
                  type == TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
  checkUnixError(fd, "creating SocketPipe's socket");
  fd_ = common::FileDescriptor(fd);

  setNonblock();

  int yes = 1;
  int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  checkUnixError(ret, "setting SO_REUSEADDR for SocketPipe");
}

Socket::Socket(event::EventLoop& loop, NetworkType networkType, SocketType type,
               int fd, SocketAddress peerAddr)
    : loop_(loop), networkType_(networkType), type_(type), fd_(fd),
      bound_(false), connected_(true), peerAddr_(new SocketAddress(peerAddr)) {
  setNonblock();
}

Socket::~Socket() { loop_.getIOConditionManager().close(fd_.fd); }

SocketAddress Socket::getPeerAddress() const {
  assertTrue(bound_ || connected_,
             "Calling getPeerAddress() on a unbound and unconnected socket.");
  assertTrue(!!peerAddr_,
             "Calling getPeerAddress() on an unprimed UDP socket.");
  return *peerAddr_.get();
}

int Socket::bind(int port) {
  assertTrue(!bound_, "Calling bind() on a bound SocketPipe");
  assertTrue(!connected_, "Calling bind() on a connected SocketPipe");

  // Binding
  SocketAddress myAddr("0.0.0.0", port);
  int ret = ::bind(fd_.fd, myAddr.asSocketAddress(), myAddr.getLength());
  checkUnixError(ret, "binding to SocketPipe's socket");

  // Getting the port that is actually used (in case that the given port is 0)
  SocketAddress myActualAddr;
  socklen_t myActualAddrLen = myActualAddr.getStorageLength();
  ret = getsockname(fd_.fd, myActualAddr.asSocketAddress(), &myActualAddrLen);
  checkUnixError(ret, "calling getsockname()");
  int actualPort = myActualAddr.getPort();

  // Listening
  if (type_ == TCP) {
    int ret = listen(fd_.fd, kSocketListenBacklog);
    checkUnixError(ret, "listening on a SocketPipe's socket");
  }

  LOG_V("Socket") << "Bound to port " << actualPort << std::endl;

  bound_ = true;
  port_ = actualPort;

  return actualPort;
}

void Socket::connect(SocketAddress peerAddr) {
  assertTrue(!connected_, "Connecting while already connected");
  assertTrue((type_ == SocketType::UDP) || !bound_,
             "Connecting while already bound");

  LOG_V("Socket") << "Connecting to " << peerAddr.getHost() << ":"
                  << peerAddr.getPort() << std::endl;

  int ret = ::connect(fd_.fd, peerAddr.asSocketAddress(), peerAddr.getLength());
  checkUnixError(ret, "connecting in SocketPipe", EINPROGRESS);
  connected_ = true;
  peerAddr_.reset(new SocketAddress(peerAddr));

  LOG_V("Socket") << "Connected to " << peerAddr.getHost() << ":"
                  << peerAddr.getPort() << std::endl;
}

size_t Socket::read(Byte* buffer, size_t capacity) {
  assertTrue(
      bound_ || connected_,
      "Trying to read from a Socket that is neither bound nor connected.");

  int ret, err;

  LOG_VV("Socket") << "Reading with buffer capacity " << capacity << std::endl;

  if (!peerAddr_) {
    assertTrue(type_ == UDP, "Trying to read from an unconnected TCP socket.");

    SocketAddress peerAddr;
    socklen_t peerAddrSize = peerAddr.getStorageLength();
    ret = recvfrom(fd_.fd, buffer, capacity, 0, peerAddr.asSocketAddress(),
                   &peerAddrSize);
    err = errno;

    if (ret >= 0) {
      connect(peerAddr);
      connected_ = true;
      peerAddr_.reset(new SocketAddress(peerAddr));
    }
  } else {
    ret = recv(fd_.fd, buffer, capacity, 0);
    err = errno;
  }

  if (type_ == TCP && ret == 0) {
    throw SocketClosedException("The socket connection is closed.");
  }

  checkSocketException(ret, err);

  if (!checkRetryableError(ret, "receiving a " +
                                    std::string(type_ == TCP ? "TCP" : "UDP") +
                                    " packet")) {
    return 0;
  }

  LOG_VV("Socket") << "Read a packet with size " << ret << std::endl;

  return ret;
}

size_t Socket::write(Byte* buffer, size_t size) {
  assertTrue(connected_, "Socket::write() called on a unconnected socket.");

  LOG_VV("Socket") << "Writing a packet with size " << size << std::endl;

  int ret = send(fd_.fd, buffer, size, 0);

  if (type_ == TCP && ret == 0 && size != 0) {
    throw SocketClosedException("The socket connection is closed.");
  }

  checkSocketException(ret, errno);

  if (!checkRetryableError(ret, "sending a " +
                                    std::string(type_ == TCP ? "TCP" : "UDP") +
                                    " packet")) {
    return 0;
  }

  return ret;
}

void Socket::setNonblock() {
  int ret = fcntl(fd_.fd, F_SETFL, fcntl(fd_.fd, F_GETFL, 0) | O_NONBLOCK);
  checkUnixError(ret, "setting O_NONBLOCK for SocketPipe");
}

void Socket::checkSocketException(int ret, int err) {
  // Just close the socket if the connection is refused (UDP), reset (TCP),
  // encountered a broken pipe, or the network is down.
  if (ret < 0 && (err == ECONNRESET || err == ECONNREFUSED || err == EPIPE ||
                  err == ENETDOWN)) {
    throw SocketClosedException(std::string(strerror(err)));
  }
}

event::Condition* Socket::canRead() const {
  return loop_.getIOConditionManager().canRead(fd_.fd);
}

event::Condition* Socket::canWrite() const {
  return loop_.getIOConditionManager().canWrite(fd_.fd);
}
} // namespace networking
