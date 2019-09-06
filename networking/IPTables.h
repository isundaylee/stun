#pragma once

#include <common/Util.h>

#include <networking/IPAddressPool.h>

#include <string>

namespace networking {

class IPTables {
public:
  static void
  masquerade(SubnetAddress const& sourceSubnet,
             std::string const& outputInterface, // TODO: use optional
             std::string const& ruleID);
  static void clear(std::string const& ruleID);

private:
  static std::string runCommandAndAssertSuccess(std::string command);
};
} // namespace networking
