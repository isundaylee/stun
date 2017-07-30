#pragma once

#include <common/Util.h>

#include <networking/IPAddressPool.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace networking {

static const std::string kIPTablesCommenClause = " -m comment --comment stun";
static const size_t kIPTablesOutputBufferSize = 1024;

class IPTables {
public:
  static void masquerade(SubnetAddress const& sourceSubnet) {
    runCommand("-t nat -A POSTROUTING -s " + sourceSubnet.toString() +
               " -j MASQUERADE" + kIPTablesCommenClause);

    LOG_T("IPTables") << "Set MASQUERADE for source " << sourceSubnet.toString()
                      << "." << std::endl;
  }

  static void clear() {
    std::string rules = runCommand("-t nat -L POSTROUTING --line-numbers");
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
      runCommand("-t nat -D POSTROUTING " + std::to_string(*it));
    }

    LOG_T("IPTables") << "Removed " << rulesToDelete.size()
                      << " iptables rules." << std::endl;
  }

private:
  static std::string runCommand(std::string command) {
    char buffer[kIPTablesOutputBufferSize];
    std::stringstream result;
    std::shared_ptr<FILE> pipe(
        popen(("/sbin/iptables " + command).c_str(), "r"), pclose);

    assertTrue(!!pipe, "Cannot popen() to run iptables command.");

    while (!feof(pipe.get())) {
      if (fgets(buffer, kIPTablesOutputBufferSize, pipe.get()) != nullptr) {
        result << buffer;
      }
    }

    return result.str();
  }
};
}
