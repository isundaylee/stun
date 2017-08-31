#pragma once

#include <json/json.hpp>

#include <common/Util.h>

#include <array>
#include <iostream>
#include <queue>
#include <set>
#include <string>

namespace networking {

using json = nlohmann::json;

struct SubnetAddress;

struct IPAddress {
public:
  IPAddress();
  explicit IPAddress(std::string const& addr);
  explicit IPAddress(uint32_t numerical);

  std::array<Byte, 4> octets;

  std::string toString() const;
  uint32_t toNumerical() const;
  IPAddress next() const;
  bool empty() const;

  bool operator==(IPAddress const& other) const;
  bool operator!=(IPAddress const& other) const;
  bool operator<(IPAddress const& other) const;

  friend std::ostream& operator<<(std::ostream& os, IPAddress const& addr);
};

void to_json(json& j, IPAddress const& addr);
void from_json(json const& j, IPAddress& addr);

struct SubnetAddress {
public:
  SubnetAddress(std::string const& subnet);
  SubnetAddress(IPAddress const& addr, int prefixLen);

  IPAddress addr;
  size_t prefixLen;

  std::string toString() const;
  std::string subnetMask() const;
  bool contains(IPAddress const& addr) const;
  IPAddress firstHostAddress() const;
  IPAddress lastHostAddress() const;
  IPAddress broadcastAddress() const;

  friend std::ostream& operator<<(std::ostream& os, SubnetAddress const& addr);

private:
  uint32_t mask;
};

void to_json(json& j, SubnetAddress const& addr);
void from_json(json const& j, SubnetAddress& addr);

class IPAddressPool {
public:
  IPAddressPool(SubnetAddress const& subnet);

  IPAddress acquire();
  void release(IPAddress const& addr);
  void reserve(IPAddress const& addr);

private:
  SubnetAddress subnet_;
  IPAddress nextAddr_;

  std::queue<IPAddress> reusables_;
  std::set<IPAddress> reserved_;
};
}
