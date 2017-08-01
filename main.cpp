#include <common/Configerator.h>
#include <common/Util.h>
#include <event/EventLoop.h>
#include <event/Timer.h>
#include <event/Trigger.h>
#include <networking/IPTables.h>
#include <networking/InterfaceConfig.h>
#include <stats/StatsManager.h>
#include <stun/CommandCenter.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace stun;

const int kStatsDumpingInterval = 1000 /* ms */;
const int kReconnectDelayInterval = 5000 /* ms */;
const int kServerPort = 2859;

std::string getConfigPath() {
  const char* homeDir = getenv("HOME");
  assertTrue(homeDir != NULL, "Cannot get $HOME.");

  return std::string(homeDir) + "/.stunrc";
}

int main(int argc, char* argv[]) {
  common::Configerator config(getConfigPath());
  event::EventLoop loop;

  // Set up periodic stats dumping
  event::Timer statsTimer(kStatsDumpingInterval);
  event::Action statsDumper({statsTimer.didFire()});
  statsDumper.callback = [&statsTimer]() {
    stats::StatsManager::dump(
        LOG_T("Stats"), [](std::string const& name, std::string const& metric) {
          return (name.find("Tunnel") == std::string::npos) &&
                 (name.find("Command") == std::string::npos) &&
                 (name.find("Center") == std::string::npos);
        });
    statsTimer.extend(kStatsDumpingInterval);
  };

  std::string role = common::Configerator::getString("role");

  LOG_T("Main") << "Running as " << role << std::endl;
  CommandCenter center;
  event::BaseCondition shouldMonitorDisconnect;
  event::Action disconnectMonitor(
      {&shouldMonitorDisconnect, center.didDisconnect()});
  event::Timer reconnectTimer{};
  event::Action reconnector({reconnectTimer.didFire()});

  if (role == "server") {
    std::string addressPool = common::Configerator::getString("address_pool");
    networking::SubnetAddress subnet(addressPool);
    networking::IPTables::masquerade(subnet);

    center.serve(kServerPort);
  } else {
    std::string server = common::Configerator::getString("server");
    center.connect(server, kServerPort);
    shouldMonitorDisconnect.fire();

    disconnectMonitor.callback = [&shouldMonitorDisconnect, &reconnectTimer]() {
      LOG() << "Will reconnect in " << kReconnectDelayInterval << " ms."
            << std::endl;
      shouldMonitorDisconnect.arm();
      reconnectTimer.reset(kReconnectDelayInterval);
    };

    reconnector.callback = [&shouldMonitorDisconnect, &reconnectTimer, &center,
                            server]() {
      LOG() << "Reconnecting..." << std::endl;
      center.connect(server, kServerPort);
      shouldMonitorDisconnect.fire();
      reconnectTimer.reset();
    };
  }

  loop.run();

  return 0;
}
