#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

#include <string>

namespace networking {

class SocketAddress {
public:
  SocketAddress();
  SocketAddress(std::string const& host, int port = 0);

  struct sockaddr* asSocketAddress() const;
  struct sockaddr_in* asInetAddress() const;

  std::string getHost() const;
  int getPort() const;
  size_t getLength() const;
  size_t getStorageLength() const;

private:
  struct sockaddr_storage storage_;
};
}
