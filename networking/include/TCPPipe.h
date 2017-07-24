#pragma once

#include <networking/Pipe.h>
#include <networking/SocketPipe.h>

#include <netdb.h>

#include <functional>

namespace networking {

struct TCPPacket : PipePacket {};

class TCPPipe : public SocketPipe<TCPPacket> {
public:
  TCPPipe() : SocketPipe(SocketType::TCP) {}

  TCPPipe(TCPPipe&& move) : SocketPipe(std::move(move)) {}

  std::function<void(TCPPipe&& client)> onAccept = [](TCPPipe&&) {};

protected:
  virtual bool read(TCPPacket& packet) override;

private:
  TCPPipe(int fd) : SocketPipe(SocketType::TCP) {
    fd_ = fd;
    connected_ = true;
    startActions();
  }
};
}
