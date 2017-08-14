#include "networking/SocketAddress.h"

#include <common/Util.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

namespace networking {

SocketAddress::SocketAddress() { memset(&storage_, 0, sizeof(storage_)); }

SocketAddress::SocketAddress(std::string const& host, int port /* = 0 */) {
  struct addrinfo hints;
  struct addrinfo* addr;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;

  int err;
  if ((err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints,
                         &addr)) != 0) {
    throwGetAddrInfoError(err);
  }

  memcpy(&storage_, addr->ai_addr, addr->ai_addrlen);
  freeaddrinfo(addr);
}

struct sockaddr* SocketAddress::asSocketAddress() const {
  return (struct sockaddr*)&storage_;
}

struct sockaddr_in* SocketAddress::asInetAddress() const {
  assertTrue(storage_.ss_family == AF_INET,
             "Unsupported sa_family: " + std::to_string(storage_.ss_family));
  return (struct sockaddr_in*)&storage_;
}

IPAddress SocketAddress::getHost() const {
  assertTrue(storage_.ss_family == AF_INET,
             "Unsupported sa_family: " + std::to_string(storage_.ss_family));
  return IPAddress(std::string(inet_ntoa(asInetAddress()->sin_addr)));
}

int SocketAddress::getPort() const {
  assertTrue(storage_.ss_family == AF_INET,
             "Unsupported sa_family: " + std::to_string(storage_.ss_family));
  return ntohs(asInetAddress()->sin_port);
}

size_t SocketAddress::getLength() const {
  assertTrue(storage_.ss_family == AF_INET,
             "Unsupported sa_family: " + std::to_string(storage_.ss_family));
  return sizeof(sockaddr_in);
}

size_t SocketAddress::getStorageLength() const { return sizeof(storage_); }
}
