#include "networking/IPAddressPool.h"

#include <common/Util.h>

#include <arpa/inet.h>
#include <sys/socket.h>

namespace networking {

IPAddress::IPAddress() {
  octets[0] = octets[1] = octets[2] = octets[3] = octets[4] = octets[5] = 0;
}

/* explicit */ IPAddress::IPAddress(in_addr addr) {
  this->type = NetworkType::IPv4;

  uint32_t haddr = ntohl(addr.s_addr);

  this->octets[0] = (haddr >> 24) & 0xff;
  this->octets[1] = (haddr >> 16) & 0xff;
  this->octets[2] = (haddr >> 8) & 0xff;
  this->octets[3] = (haddr >> 0) & 0xff;
}

/* explicit */ IPAddress::IPAddress(in6_addr addr) {
  this->type = NetworkType::IPv6;

  memcpy(this->octets.data(), addr.s6_addr, sizeof(addr.s6_addr));
}

IPAddress::IPAddress(std::string const& addr, NetworkType type) {
  assertTrue(type == NetworkType::IPv4 || type == NetworkType::IPv6,
             "Unsupported network type: " + std::to_string(type));
  this->type = type;

  int ret = inet_pton((type == NetworkType::IPv4 ? AF_INET : AF_INET6),
                      addr.c_str(), octets.data());
  if (ret == 0) {
    assertTrue(false, "Invalid IP address: " + addr);
  } else if (ret != 1) {
    throwUnixError("in IPAddress::IPAddress()");
  }
}

IPAddress::IPAddress(uint32_t numerical, NetworkType type) {
  assertTrue(
      type == NetworkType::IPv4,
      "IPAddress::IPAddress(uint32_t) supported for IPv4 addresses only.");

  this->type = type;

  octets[0] = (numerical & 0xff000000) >> 24;
  octets[1] = (numerical & 0x00ff0000) >> 16;
  octets[2] = (numerical & 0x0000ff00) >> 8;
  octets[3] = (numerical & 0x000000ff);
}

std::string IPAddress::toString() const {
  char buf[INET6_ADDRSTRLEN];

  const char* ret =
      inet_ntop((this->type == NetworkType::IPv4 ? AF_INET : AF_INET6),
                this->octets.data(), buf, INET6_ADDRSTRLEN);

  return std::string(ret);
}

uint32_t IPAddress::toNumerical() const {
  return (((uint32_t)octets[0]) << 24) + (((uint32_t)octets[1]) << 16) +
         (((uint32_t)octets[2]) << 8) + ((uint32_t)octets[3]);
}

IPAddress IPAddress::next() const {
  return IPAddress(toNumerical() + 1, this->type);
}

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
  // FIXME: Assuming IPv4 address from json
  addr = IPAddress(j.get<std::string>(), IPv4);
}

SubnetAddress::SubnetAddress(std::string const& subnet) {
  std::size_t slashPos = subnet.find('/');
  assertTrue(slashPos != std::string::npos, "Invalid subnet: " + subnet);

  addr = IPAddress(subnet.substr(0, slashPos), IPv4);
  prefixLen = std::stoi(subnet.substr(slashPos + 1));

  assertTrue(1 <= prefixLen && prefixLen <= 32,
             "Invalid subnet prefix len: " + std::to_string(prefixLen));

  mask = ((1ULL << prefixLen) - 1) << (32 - prefixLen);
}

SubnetAddress::SubnetAddress(IPAddress const& addr, int prefixLen)
    : addr(addr) {
  this->prefixLen = prefixLen;
  mask = ((1ULL << prefixLen) - 1) << (32 - prefixLen);
}

std::string SubnetAddress::toString() const {
  return addr.toString() + "/" + std::to_string(prefixLen);
}

std::string SubnetAddress::subnetMask() const {
  return std::to_string((mask & 0xff000000) >> 24) + "." +
         std::to_string((mask & 0x00ff0000) >> 16) + "." +
         std::to_string((mask & 0x0000ff00) >> 8) + "." +
         std::to_string((mask & 0x000000ff) >> 0);
}

bool SubnetAddress::contains(IPAddress const& addr) const {
  return (this->addr.toNumerical() & mask) == (addr.toNumerical() & mask);
}

IPAddress SubnetAddress::firstHostAddress() const {
  return IPAddress((addr.toNumerical() & mask) + 1, IPv4);
}

IPAddress SubnetAddress::lastHostAddress() const {
  // -1 to exclude the broadcast address
  return IPAddress((addr.toNumerical() & mask) + (~mask) - 1, IPv4);
}

IPAddress SubnetAddress::broadcastAddress() const {
  return IPAddress((addr.toNumerical() & mask) + (~mask), IPv4);
}

/* friend */ std::ostream& operator<<(std::ostream& os,
                                      SubnetAddress const& addr) {
  os << addr.toString();
  return os;
}

void to_json(json& j, SubnetAddress const& addr) { j = addr.toString(); }

void from_json(json const& j, SubnetAddress& addr) {
  addr = SubnetAddress(j.get<std::string>());
}

IPAddressPool::IPAddressPool(SubnetAddress const& subnet) : subnet_(subnet) {
  nextAddr_ = subnet_.firstHostAddress();
}

IPAddress IPAddressPool::acquire() {
  if (!reusables_.empty()) {
    auto addr = reusables_.front();
    reusables_.pop();

    LOG_V("Address") << "Reusing " << addr << " from the address pool"
                     << std::endl;

    return addr;
  }

  auto addr = IPAddress{};

  do {
    addr = nextAddr_;
    nextAddr_ = nextAddr_.next();
  } while (addr != subnet_.broadcastAddress() && (reserved_.count(addr) > 0));

  assertTrue(addr != subnet_.broadcastAddress(), "Ran out of addresses.");

  LOG_V("Address") << "Using new address " << addr << " from the address pool"
                   << std::endl;

  return addr;
}

void IPAddressPool::release(IPAddress const& addr) {
  auto found = reserved_.find(addr);
  assertTrue(found == reserved_.end(), "Reserved address " + addr.toString() +
                                           " released to IPAddressPool.");

  LOG_V("Address") << "Releasing " << addr << " to the address pool"
                   << std::endl;

  reusables_.push(addr);
}

void IPAddressPool::reserve(IPAddress const& addr) {
  assertTrue(subnet_.contains(addr),
             "Trying to reserve out-of-pool address: " + addr.toString());

  auto inserted = reserved_.insert(addr).second;
  assertTrue(inserted, "Address reserved more than once: " + addr.toString());
}
} // namespace networking
