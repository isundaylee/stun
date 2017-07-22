#pragma once

#include <UDPConnection.h>

#include <ev/ev++.h>
#include <netdb.h>

#include <functional>

namespace stun {

const int kUDPPacketBufferSize = 2048;

struct UDPPacket {
  size_t size;
  char data[kUDPPacketBufferSize];
};

class UDPServer {
public:
  UDPServer(int port);
  ~UDPServer();

  void bind();

  std::function<void (UDPPacket const&)> onReceive = [](UDPPacket const&) {};

private:
  UDPServer(UDPServer const& copy) = delete;
  UDPServer& operator=(UDPServer const& copy) = delete;

  void doReceive(ev::io& watcher, int events);

  ev::io io_;
  struct addrinfo *myAddr_;
  int socket_;
};

}
