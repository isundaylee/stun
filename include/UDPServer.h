#pragma once

#include <UDPConnection.h>

#include <netdb.h>

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
  UDPPacket receivePacket();

private:
  UDPServer(UDPServer const& copy) = delete;
  UDPServer& operator=(UDPServer const& copy) = delete;

  struct addrinfo *myAddr_;
  int socket_;
};

}
