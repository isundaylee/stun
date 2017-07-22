#pragma once

#include <FIFO.h>

#include <ev/ev++.h>
#include <netdb.h>

#include <functional>

namespace stun {

const int kUDPPacketBufferSize = 2048;
const int kUDPInboundQueueSize = 32;
const int kUDPOutboundQueueSize = 32;

struct UDPPacket {
  size_t size;
  char data[kUDPPacketBufferSize];
};

class UDPServer {
public:
  UDPServer();
  ~UDPServer();

  void bind(int port);
  void connect(std::string const& host, int port);

  FIFO<UDPPacket> inboundQ;
  FIFO<UDPPacket> outboundQ;

private:
  UDPServer(UDPServer const& copy) = delete;
  UDPServer& operator=(UDPServer const& copy) = delete;

  struct addrinfo* getAddr(std::string const& host, int port);

  void doReceive(ev::io& watcher, int events);
  void doSend(ev::io& watcher, int events);

  ev::io receiveWatcher_;
  ev::io sendWatcher_;

  bool connected_;
  int socket_;
};

}
