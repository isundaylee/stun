#pragma once

#include <networking/Packet.h>
#include <networking/Pipe.h>
#include <networking/SocketPipe.h>

#include <netdb.h>

#include <functional>

namespace networking {

const size_t kTCPPacketSize = 2048;

struct TCPPacket : Packet<kTCPPacketSize> {};

class TCPPipe : public SocketPipe<TCPPacket> {
public:
  TCPPipe() : SocketPipe(SocketType::TCP) {}

  TCPPipe(TCPPipe&& move) : SocketPipe(std::move(move)) {}

  std::function<void(TCPPipe&& client)> onAccept = [](TCPPipe&&) {};

protected:
  virtual bool read(TCPPacket& packet) override;

private:
  TCPPipe(int fd, std::string peerAddr, int peerPort)
      : SocketPipe(SocketType::TCP) {
    fd_ = fd;
    connected_ = true;
    this->peerAddr = peerAddr;
    this->peerPort = peerPort;
    startActions();
  }
};
}
