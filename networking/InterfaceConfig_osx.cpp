#include "networking/InterfaceConfig.h"

#include <common/Util.h>

#include <sstream>

#if OSX

namespace networking {

static const std::string kInterfaceConfigIfconfigPath = "/sbin/ifconfig";
static const std::string kInterfaceConfigRoutePath = "/sbin/route";

InterfaceConfig::InterfaceConfig() {}

InterfaceConfig::~InterfaceConfig() {}

void InterfaceConfig::newLink(std::string const& deviceName, unsigned int mtu) {
  std::string command = kInterfaceConfigIfconfigPath + " " + deviceName +
                        " mtu " + std::to_string(mtu);
  runCommand(command);
}

void InterfaceConfig::setLinkAddress(std::string const& deviceName,
                                     std::string const& localAddress,
                                     std::string const& peerAddress) {
  std::string command = kInterfaceConfigIfconfigPath + " " + deviceName +
                        " inet " + localAddress + " " + peerAddress;
  runCommand(command);
}

void InterfaceConfig::newRoute(SubnetAddress const& destSubnet,
                               RouteDestination const& routeDest) {
  std::string command = kInterfaceConfigRoutePath + " -n add -net " +
                        destSubnet.addr + "/" +
                        std::to_string(destSubnet.prefixLen);

  assertTrue(!routeDest.gatewayAddr.empty(),
             "Only gateway routes are currently supported on OSX.");
  command += " " + routeDest.gatewayAddr;

  runCommand(command);

  LOG_T("Interface") << "Added a route to " << destSubnet.toString() << " via "
                     << routeDest.gatewayAddr << "." << std::endl;
}

RouteDestination InterfaceConfig::getRoute(std::string const& destAddr) {
  std::string output =
      runCommand(kInterfaceConfigRoutePath + " -n get " + destAddr);
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

  LOG_T("Interface") << "Route to " << destAddr << " is through "
                     << interface + " via " << gateway << "." << std::endl;

  return RouteDestination(interface, gateway);
}
}

#endif
