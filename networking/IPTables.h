#pragma once

#include <common/Util.h>

#include <networking/IPAddressPool.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace networking {

static const std::string kIPTablesCommonClause = " -m comment --comment stun";
static const size_t kIPTablesOutputBufferSize = 1024;

class IPTables {
public:
  static void masquerade(SubnetAddress const& sourceSubnet) {
    runCommandAndAssertSuccess("-t nat -A POSTROUTING -s " +
                               sourceSubnet.toString() + " -j MASQUERADE" +
                               kIPTablesCommonClause);

    LOG_V("IPTables") << "Set MASQUERADE for source " << sourceSubnet.toString()
                      << "." << std::endl;
  }

  static void clear() {
    std::string rules =
        runCommandAndAssertSuccess("-t nat -L POSTROUTING --line-numbers -n");
    std::stringstream ss(rules);
    std::string line;

    std::vector<int> rulesToDelete;

    while (std::getline(ss, line, '\n')) {
      std::size_t pos = line.find("/* stun */");
      if (pos != std::string::npos) {
        std::size_t spacePos = line.find(" ");
        assertTrue(spacePos != std::string::npos,
                   "Cannot parse iptables rulenum.");
        rulesToDelete.push_back(std::stoi(line.substr(0, spacePos)));
      }
    }

    for (auto it = rulesToDelete.rbegin(); it != rulesToDelete.rend(); it++) {
      runCommandAndAssertSuccess("-t nat -D POSTROUTING " +
                                 std::to_string(*it));
    }

    LOG_V("IPTables") << "Removed " << rulesToDelete.size()
                      << " iptables rules." << std::endl;
  }

private:
  static std::string runCommandAndAssertSuccess(std::string command) {
#if !TARGET_LINUX
    throw std::runtime_error("IPTables only supports Linux platforms.");
#endif

    return ::runCommandAndAssertSuccess("/sbin/iptables " + command).stdout;
  }
};
} // namespace networking
