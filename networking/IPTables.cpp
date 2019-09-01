#include "networking/IPTables.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace networking {

/* static */ void IPTables::masquerade(SubnetAddress const& sourceSubnet,
                                       std::string const& ruleID) {
  auto ruleComment = "stun " + ruleID;
  runCommandAndAssertSuccess("-t nat -A POSTROUTING -s " +
                             sourceSubnet.toString() + " -j MASQUERADE " +
                             "-m comment --comment \"" + ruleComment + "\"");

  LOG_V("IPTables") << "Set MASQUERADE for source " << sourceSubnet.toString()
                    << "." << std::endl;
}

/* static */ void IPTables::clear(std::string const& ruleID) {
  std::string rules =
      runCommandAndAssertSuccess("-t nat -L POSTROUTING --line-numbers -n");
  std::stringstream ss(rules);
  std::string line;

  std::vector<int> rulesToDelete;

  while (std::getline(ss, line, '\n')) {
    std::size_t pos = line.find("/* stun " + ruleID + " */");
    if (pos != std::string::npos) {
      std::size_t spacePos = line.find(" ");
      assertTrue(spacePos != std::string::npos,
                 "Cannot parse iptables rulenum.");
      rulesToDelete.push_back(std::stoi(line.substr(0, spacePos)));
    }
  }

  for (auto it = rulesToDelete.rbegin(); it != rulesToDelete.rend(); it++) {
    runCommandAndAssertSuccess("-t nat -D POSTROUTING " + std::to_string(*it));
  }

  LOG_V("IPTables") << "Removed " << rulesToDelete.size() << " iptables rules."
                    << std::endl;
}

/* static */ std::string
IPTables::runCommandAndAssertSuccess(std::string command) {
#if !TARGET_LINUX
  throw std::runtime_error("IPTables only supports Linux platforms.");
#endif

  return ::runCommandAndAssertSuccess("/sbin/iptables " + command).stdout;
}

}; // namespace networking