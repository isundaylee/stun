#include <common/Configerator.h>
#include <common/Util.h>

#include <stun/CommandCenter.h>

#include <networking/IPTables.h>
#include <networking/InterfaceConfig.h>

#include <event/EventLoop.h>
#include <event/Timer.h>
#include <stats/StatsManager.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace stun;

const int kStatsDumpingInterval = 1000 /* ms */;

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
          return (name.find("Data") == std::string::npos) &&
                 (name.find("Command") == std::string::npos) &&
                 (name.find("Center") == std::string::npos);
        });
    statsTimer.extend(kStatsDumpingInterval);
  };

  std::string role = common::Configerator::getString("role");

  LOG_T("Main") << "Running as " << role << std::endl;
  CommandCenter center;
  if (role == "server") {
    std::string addressPool = common::Configerator::getString("address_pool");
    networking::SubnetAddress subnet(addressPool);
    networking::IPTables::masquerade(subnet);

    center.serve(2859);
  } else {
    std::string server = common::Configerator::getString("server");
    center.connect(server, 2859);
  }

  loop.run();

  return 0;
}
