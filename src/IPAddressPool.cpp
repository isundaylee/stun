#include "IPAddressPool.h"

#include <common/Util.h>

#include <arpa/inet.h>

namespace stun {

const uint32_t kLastOctetMask = 0xff;

IPAddressPool::IPAddressPool(std::string const& subnet, size_t prefixLen) :
    prefixLen_(prefixLen),
    subnetMask_(((1U << prefixLen) - 1) << (32 - prefixLen)),
    hostMask_(~subnetMask_) {
  assertTrue(1 <= prefixLen && prefixLen <= 32, "Invalid prefixLen" + std::to_string(prefixLen));
  inet_pton(AF_INET, subnet.c_str(), &subnet_);
  current_ = subnet_;
}

std::string IPAddressPool::acquire() {
  char *octets = (char*) &current_;

  assertTrue(octets[3] < 0xff, "TODO: do a proper address increment");
  octets[3] ++;
  
  std::string addr = std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
      std::to_string(octets[2]) + "." + std::to_string(octets[3]);

  return addr;
}

}
