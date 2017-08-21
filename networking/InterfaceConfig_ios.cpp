#include "networking/InterfaceConfig.h"

#include <common/Util.h>

#include <sstream>

#if IOS

namespace networking {

InterfaceConfig::InterfaceConfig() {}

InterfaceConfig::~InterfaceConfig() {}

/* static */ void InterfaceConfig::newLink(std::string const& deviceName,
                                           unsigned int mtu) {
  notImplemented("InterfaceConfig not supported on iOS.");
}

/* static */ void
InterfaceConfig::setLinkAddress(std::string const& deviceName,
                                IPAddress const& localAddress,
                                IPAddress const& peerAddress) {
  notImplemented("InterfaceConfig not supported on iOS.");
}

/* static */ void InterfaceConfig::newRoute(Route const& route) {
  notImplemented("InterfaceConfig not supported on iOS.");
}

/* static */ RouteDestination
InterfaceConfig::getRoute(IPAddress const& destAddr) {
  notImplemented("InterfaceConfig not supported on iOS.");
}

/* static */ InterfaceConfig& InterfaceConfig::getInstance() {
  static InterfaceConfig instance;
  return instance;
}
}

#endif
