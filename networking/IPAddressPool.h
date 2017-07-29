#pragma once

#include <queue>
#include <string>

namespace networking {

class SubnetAddress {
public:
  SubnetAddress(std::string const& subnet);
  SubnetAddress(std::string const& addr, int prefixLen);

  std::string toString() const;

  std::string addr;
  size_t prefixLen;
};

class IPAddressPool {
public:
  IPAddressPool(SubnetAddress const& subnet);

  std::string acquire();
  void release(std::string const& addr);

private:
  uint32_t subnet_;
  uint32_t current_;

  std::queue<std::string> reusables_;
};
}
