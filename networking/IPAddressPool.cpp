#include "networking/IPAddressPool.h"

#include <common/Util.h>

#include <arpa/inet.h>

namespace networking {

IPAddress::IPAddress() { octets[0] = octets[1] = octets[2] = octets[3] = 0; }

IPAddress::IPAddress(std::string const& addr) {
  auto tokens = split(addr, ".");
  assertTrue(tokens.size() == 4, "Invalid IP address: " + addr);

  for (auto i = 0; i < tokens.size(); i++) {
    assertTrue(std::stoi(tokens[i]) >= 0, "Invalid IP address: " + addr);
    assertTrue(std::stoi(tokens[i]) <= 255, "Invalid IP address: " + addr);
    octets[i] = std::stoi(tokens[i]);
  }
}

IPAddress::IPAddress(uint32_t numerical) {
  octets[0] = (numerical & 0xff000000) >> 24;
  octets[1] = (numerical & 0x00ff0000) >> 16;
  octets[2] = (numerical & 0x0000ff00) >> 8;
  octets[3] = (numerical & 0x000000ff);
}

std::string IPAddress::toString() const {
  return std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
         std::to_string(octets[2]) + "." + std::to_string(octets[3]);
}

uint32_t IPAddress::toNumerical() const {
  return (((uint32_t)octets[0]) << 24) + (((uint32_t)octets[1]) << 16) +
         (((uint32_t)octets[2]) << 8) + ((uint32_t)octets[3]);
}

IPAddress IPAddress::next() const { return IPAddress(toNumerical() + 1); }

bool IPAddress::empty() const { return toNumerical() == 0; }

bool IPAddress::operator==(IPAddress const& other) const {
  return octets == other.octets;
}

bool IPAddress::operator!=(IPAddress const& other) const {
  return !(*this == other);
}

bool IPAddress::operator<(IPAddress const& other) const {
  return toNumerical() < other.toNumerical();
}

/* friend */ std::ostream& operator<<(std::ostream& os, IPAddress const& addr) {
  os << addr.toString();
  return os;
}

void to_json(json& j, IPAddress const& addr) { j = addr.toString(); }

void from_json(json const& j, IPAddress& addr) {
  addr = IPAddress(j.get<std::string>());
}

SubnetAddress::SubnetAddress(std::string const& subnet) {
  std::size_t slashPos = subnet.find('/');
  assertTrue(slashPos != std::string::npos, "Invalid subnet: " + subnet);

  addr = subnet.substr(0, slashPos);
  prefixLen = std::stoi(subnet.substr(slashPos + 1));

  assertTrue(1 <= prefixLen && prefixLen <= 32,
             "Invalid subnet prefix len: " + std::to_string(prefixLen));

  mask = ((1ULL << prefixLen) - 1) << (32 - prefixLen);
}

SubnetAddress::SubnetAddress(std::string const& addr, int prefixLen)
    : addr(addr) {
  this->prefixLen = prefixLen;
}

std::string SubnetAddress::toString() const {
  return addr.toString() + "/" + std::to_string(prefixLen);
}

bool SubnetAddress::contains(IPAddress const& addr) const {
  return (this->addr.toNumerical() & mask) == (addr.toNumerical() & mask);
}

IPAddress SubnetAddress::firstHostAddress() const {
  return IPAddress((addr.toNumerical() & mask) + 1);
}

IPAddress SubnetAddress::lastHostAddress() const {
  // -1 to exclude the broadcast address
  return IPAddress((addr.toNumerical() & mask) + (~mask) - 1);
}

IPAddress SubnetAddress::broadcastAddress() const {
  return IPAddress((addr.toNumerical() & mask) + (~mask));
}

/* friend */ std::ostream& operator<<(std::ostream& os,
                                      SubnetAddress const& addr) {
  os << addr.toString();
  return os;
}

IPAddressPool::IPAddressPool(SubnetAddress const& subnet) : subnet_(subnet) {
  nextAddr_ = subnet_.firstHostAddress();
}

IPAddress IPAddressPool::acquire() {
  if (!reusables_.empty()) {
    auto addr = reusables_.front();
    reusables_.pop();
    return addr;
  }

  auto addr = IPAddress{};

  do {
    addr = nextAddr_;
    nextAddr_ = nextAddr_.next();
  } while (addr != subnet_.broadcastAddress() && (reserved_.count(addr) > 0));

  assertTrue(addr != subnet_.broadcastAddress(), "Ran out of addresses.");

  return addr;
}

void IPAddressPool::release(IPAddress const& addr) { reusables_.push(addr); }

void IPAddressPool::reserve(IPAddress const& addr) {
  assertTrue(subnet_.contains(addr),
             "Trying to reserve out-of-pool address: " + addr.toString());

  auto inserted = reserved_.insert(addr).second;
  assertTrue(inserted, "Address reserved more than once: " + addr.toString());
}
}
