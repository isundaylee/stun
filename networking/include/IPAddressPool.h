#pragma once

#include <string>

namespace networking {

class IPAddressPool {
public:
  IPAddressPool(std::string const& subnet, size_t preixLen);

  std::string acquire();
  void release(std::string const& addr);

private:
  uint32_t subnet_;
  uint32_t current_;
  uint32_t subnetMask_;
  uint32_t hostMask_;
  size_t prefixLen_;
};
}
