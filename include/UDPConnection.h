#pragma once

#include <netdb.h>

#include <string>

namespace stun {

class UDPConnection {
public:
  explicit UDPConnection(int socket);
  UDPConnection(std::string const& host, int port);

private:
  struct addrinfo *hostAddr;
  int socket_;
};

}
