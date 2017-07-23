#pragma once

#include <FIFO.h>
#include <Pipe.h>
#include <SocketPipe.h>

#include <ev/ev++.h>
#include <netdb.h>

#include <functional>

namespace stun {

struct TCPPacket: PipePacket {};

class TCPPipe: public SocketPipe<TCPPacket> {
public:
  TCPPipe() :
    SocketPipe(SocketType::TCP) {}

  TCPPipe(TCPPipe&& move) :
    SocketPipe(std::move(move)) {}

  std::function<void (TCPPipe&& client)> onAccept = [](TCPPipe&&) {};

protected:
  virtual bool read(TCPPacket &packet) override;

private:
  TCPPipe(int fd) :
      SocketPipe(SocketType::TCP) {
    fd_ = fd;
    connected_ = true;
    shouldOutputStats = true;
  }
};

}
