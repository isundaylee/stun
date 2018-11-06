#include "networking/InterfaceConfig.h"

#include <common/Util.h>

#include <regex>
#include <sstream>

#if TARGET_OSX || TARGET_BSD

static const std::string kInterfaceConfigIfconfigPath = "/sbin/ifconfig";
static const std::string kInterfaceConfigRoutePath = "/sbin/route";
static const std::string kInterfaceConfigNetworkSetupPath =
    "/usr/sbin/networksetup";

namespace networking {

InterfaceConfig::InterfaceConfig() {}

InterfaceConfig::~InterfaceConfig() {}

/* static */ void InterfaceConfig::newLink(std::string const& deviceName,
                                           unsigned int mtu) {
  std::string command = kInterfaceConfigIfconfigPath + " " + deviceName +
                        " mtu " + std::to_string(mtu);
  runCommandAndAssertSuccess(command);
}

/* static */ void
InterfaceConfig::setLinkAddress(std::string const& deviceName,
                                IPAddress const& localAddress,
                                IPAddress const& peerAddress) {
  assertTrue(localAddress.type == NetworkType::IPv4 &&
                 peerAddress.type == NetworkType::IPv4,
             "InterfaceConfig supports IPv4 addresses only.");

  std::string command = kInterfaceConfigIfconfigPath + " " + deviceName +
                        " inet " + localAddress.toString() + " " +
                        peerAddress.toString();
  runCommandAndAssertSuccess(command);
}

/* static */ void InterfaceConfig::newRoute(Route const& route) {
  if (route.dest.gatewayAddr.empty()) {
    assertTrue(false, "Interface routes are not supported on OSX/BSD yet.");
  }

  assertTrue(route.dest.gatewayAddr.type == NetworkType::IPv4,
             "InterfaceConfig supports IPv4 addresses only.");

  std::string command = kInterfaceConfigRoutePath + " -n add -net " +
                        route.subnet.addr.toString() + "/" +
                        std::to_string(route.subnet.prefixLen) + " " +
                        route.dest.gatewayAddr.toString();

#ifdef TARGET_OSX
  runCommandAndAssertSuccess(command);
#elif TARGET_BSD
  runCommand(command);
#endif

  LOG_V("Interface") << "Added a route to " << route.subnet.toString()
                     << " via " << route.dest.gatewayAddr << std::endl;
}

/* static */ void InterfaceConfig::deleteRoute(const SubnetAddress& dest) {
  std::string command =
      kInterfaceConfigRoutePath + " delete " + dest.toString();

#ifdef TARGET_OSX
  runCommandAndAssertSuccess(command);
#elif TARGET_BSD
  runCommand(command);
#endif

  LOG_V("Interface") << "Removed existing route to " << dest.toString()
                     << std::endl;
}

/* static */ RouteDestination
InterfaceConfig::getRoute(IPAddress const& destAddr) {
  assertTrue(destAddr.type == NetworkType::IPv4,
             "InterfaceConfig supports IPv4 addresses only.");

  std::string output =
      runCommandAndAssertSuccess(kInterfaceConfigRoutePath + " -n get " +
                                 destAddr.toString())
          .stdout;
  std::stringstream data(output);

  // Sample output:
  //    route to: 47.52.102.136
  // destination: 47.52.102.136
  //     gateway: 10.0.0.1
  //   interface: en0
  //       flags: <UP,GATEWAY,HOST,DONE,WASCLONED,IFSCOPE,IFREF>
  // recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount      mtu expire
  //       0         0         0       174        13         0      1500 0

  std::string gateway = "";
  std::string interface = "";

  while (!data.eof()) {
    std::string line;
    std::getline(data, line);

    if (line.find("gateway:") != std::string::npos) {
      size_t colonPos = line.find(":");
      gateway = line.substr(colonPos + 2);
    }

    if (line.find("interface:") != std::string::npos) {
      size_t colonPos = line.find(":");
      interface = line.substr(colonPos + 2);
    }
  }

  LOG_V("Interface") << "Route to " << destAddr << " is through "
                     << interface + " via " << gateway << "." << std::endl;

  if (gateway.empty()) {
    return RouteDestination(interface, IPAddress());
  } else {
    return RouteDestination(interface, IPAddress(gateway, NetworkType::IPv4));
  }
}

/* static */ InterfaceConfig& InterfaceConfig::getInstance() {
  static InterfaceConfig instance;
  return instance;
}

#if TARGET_OSX

static std::string getFullInterfaceName(std::string const& interfaceName) {
  // First find the full interface name matching the given interfaceName
  std::string out =
      runCommandAndAssertSuccess(kInterfaceConfigNetworkSetupPath +
                                 " -listnetworkserviceorder")
          .stdout;

  std::regex lineRegex("Hardware Port: (.+), Device: " + interfaceName);
  std::smatch match;
  if (!std::regex_search(out, match, lineRegex)) {
    assertTrue(false, "Cannot find full name of interface " + interfaceName);
  }

  return match[1].str();
}

/* static */ std::vector<networking::IPAddress>
InterfaceConfig::getDNSServers(std::string const& interfaceName) {
  auto fullInterfaceName = getFullInterfaceName(interfaceName);

  // Find DNS servers
  auto out = runCommandAndAssertSuccess(kInterfaceConfigNetworkSetupPath +
                                        " -getdnsservers " + "\"" +
                                        fullInterfaceName + "\"")
                 .stdout;

  if (out ==
      "There aren't any DNS Servers set on " + fullInterfaceName + ".\n") {
    return {};
  } else {
    auto servers = std::vector<networking::IPAddress>{};

    size_t index = 0;
    while (true) {
      auto next = out.find('\n', index);

      if (next == std::string::npos) {
        break;
      }

      servers.push_back(
          IPAddress{out.substr(index, next - index), NetworkType::IPv4});

      index = next + 1;
    }

    return servers;
  }
}

/* static */ void
InterfaceConfig::setDNSServers(std::string const& interfaceName,
                               std::vector<networking::IPAddress> servers) {
  auto fullInterfaceName = getFullInterfaceName(interfaceName);

  auto command = kInterfaceConfigNetworkSetupPath + " -setdnsservers " + "\"" +
                 fullInterfaceName + "\"";

  for (auto const& server : servers) {
    command += " " + server.toString();
  }

  if (servers.empty()) {
    command += " empty";
  }

  runCommandAndAssertSuccess(command);
}

#endif

} // namespace networking

#endif
