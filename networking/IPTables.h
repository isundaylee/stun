#pragma once

#include <common/Util.h>

#include <networking/IPAddressPool.h>

#include <stdexcept>
#include <string>

namespace networking {

class IPTables {
public:
  static void masquerade(SubnetAddress const& sourceSubnet) {
    runCommand("-t nat -A POSTROUTING -s " + sourceSubnet.toString() +
               " -j MASQUERADE");

    LOG() << "Successfully set iptables MASQUERADE for traffic from subnet "
          << sourceSubnet.toString() << "." << std::endl;
  }

private:
  static void runCommand(std::string command) {
    int ret = system(("/sbin/iptables " + command).c_str());
    if (ret != 0) {
      throw std::runtime_error("Non-zero return code while executing '" +
                               command + "': " + std::to_string(ret));
    }
  }
};
}
