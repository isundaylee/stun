#include "networking/IPAddressPool.h"

#include <common/Util.h>

#include <arpa/inet.h>

namespace networking {

SubnetAddress::SubnetAddress(std::string const& subnet) {
  std::size_t slashPos = subnet.find('/');
  assertTrue(slashPos != std::string::npos, "Invalid subnet: " + subnet);

  addr = subnet.substr(0, slashPos);
  prefixLen = std::stoi(subnet.substr(slashPos + 1));
}

SubnetAddress::SubnetAddress(std::string const& addr, int prefixLen) {
  this->addr = addr;
  this->prefixLen = prefixLen;
}

std::string SubnetAddress::toString() const {
  return addr + "/" + std::to_string(prefixLen);
}

IPAddressPool::IPAddressPool(SubnetAddress const& subnet) {
  assertTrue(1 <= subnet.prefixLen && subnet.prefixLen <= 32,
             "Invalid prefixLen" + std::to_string(subnet.prefixLen));
  inet_pton(AF_INET, subnet.addr.c_str(), &subnet_);
  current_ = subnet_;
}

std::string IPAddressPool::acquire() {
  if (!reusables_.empty()) {
    std::string addr = reusables_.front();
    reusables_.pop();
    return addr;
  }

  char* octets = (char*)&current_;

  assertTrue(octets[3] < 0x7f, "TODO: do a proper address increment");
  octets[3]++;

  std::string addr =
      std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
      std::to_string(octets[2]) + "." + std::to_string(octets[3]);

  return addr;
}

void IPAddressPool::release(std::string const& addr) { reusables_.push(addr); }
}
