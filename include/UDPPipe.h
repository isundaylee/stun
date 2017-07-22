#pragma once

#include <FIFO.h>
#include <Pipe.h>

#include <ev/ev++.h>
#include <netdb.h>

#include <functional>

namespace stun {

const int kUDPPacketBufferSize = 4096;
const int kUDPInboundQueueSize = 32;
const int kUDPOutboundQueueSize = 32;

struct UDPPacket {
  size_t size;
  char data[kUDPPacketBufferSize];
};

class UDPPipe: public Pipe<UDPPacket> {
public:
  UDPPipe();
  ~UDPPipe();

  void bind(int port);
  void connect(std::string const& host, int port);

private:
  UDPPipe(UDPPipe const& copy) = delete;
  UDPPipe& operator=(UDPPipe const& copy) = delete;

  struct addrinfo* getAddr(std::string const& host, int port);

  bool read(UDPPacket& packet) override;
  bool write(UDPPacket const& packet) override;

  bool connected_;
};

}
