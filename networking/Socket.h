#pragma once

#include <common/FileDescriptor.h>
#include <common/Util.h>
#include <event/Condition.h>
#include <networking/SocketAddress.h>

#include <memory>

namespace networking {

class SocketClosedException : public std::runtime_error {
public:
  SocketClosedException(std::string const& reason)
      : std::runtime_error(reason) {}
};

enum SocketType { TCP, UDP };

class Socket {
public:
  Socket(SocketType type);
  Socket(SocketType type, int fd, SocketAddress peerAddr);

  Socket(Socket&& move) = default;
  Socket& operator=(Socket&& move) = default;

  int bind(int port);
  void connect(SocketAddress peer);
  SocketAddress getPeerAddress() const;
  size_t read(Byte* buffer, size_t capacity);
  size_t write(Byte* buffer, size_t size);

  event::Condition* canRead() const;
  event::Condition* canWrite() const;

protected:
  SocketType type_;
  common::FileDescriptor fd_;
  bool bound_;
  bool connected_;
  // TODO: UGLY AS HELL!!
  std::unique_ptr<SocketAddress> peerAddr_;

private:
  Socket(Socket const& copy) = delete;
  Socket& operator=(Socket const& copy) = delete;

  void setNonblock();
};
}
