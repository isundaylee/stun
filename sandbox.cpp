#include "networking/IPAddressPool.h"

#include <iostream>

using namespace std::chrono_literals;
using namespace networking;

int main(int argc, char* argv[]) {
  std::cout << IPAddress("2001:2::aab1:14cd:e28d:dbfe:5be8", IPv6).toString() << std::endl;

  return 0;
}
