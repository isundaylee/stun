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
  hints.ai_family = PF_UNSPEC;

  int err;
  if ((err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints,
                         &addr)) != 0) {
    throwGetAddrInfoError(err);
  }

  memcpy(&storage_, addr->ai_addr, addr->ai_addrlen);
  freeaddrinfo(addr);

  this->type = (addr->ai_family == AF_INET ? IPv4 : IPv6);
}

struct sockaddr* SocketAddress::asSocketAddress() const {
  return (struct sockaddr*)&storage_;
}

IPAddress SocketAddress::getHost() const {
  assertTrue(storage_.ss_family == AF_INET || storage_.ss_family == AF_INET6,
             "Unsupported sa_family: " + std::to_string(storage_.ss_family));

  if (storage_.ss_family == AF_INET) {
    return IPAddress(((sockaddr_in*)&storage_)->sin_addr);
  } else {
    return IPAddress(((sockaddr_in6*)&storage_)->sin6_addr);
  }
}

int SocketAddress::getPort() const {
  assertTrue(storage_.ss_family == AF_INET || storage_.ss_family == AF_INET6,
             "Unsupported sa_family: " + std::to_string(storage_.ss_family));

  if (storage_.ss_family == AF_INET) {
    return ntohs(((sockaddr_in*)&storage_)->sin_port);
  } else {
    return ntohs(((sockaddr_in6*)&storage_)->sin6_port);
  }
}

size_t SocketAddress::getLength() const {
  assertTrue(storage_.ss_family == AF_INET || storage_.ss_family == AF_INET6,
             "Unsupported sa_family: " + std::to_string(storage_.ss_family));

  return this->type == NetworkType::IPv4 ? sizeof(sockaddr_in)
                                         : sizeof(sockaddr_in6);
}

size_t SocketAddress::getStorageLength() const { return sizeof(storage_); }
}
