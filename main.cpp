#include <common/Util.h>
#include <stun/CommandCenter.h>

#include <event/EventLoop.h>

#include <event/Timer.h>
#include <stats/StatsManager.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

using namespace stun;

const int kStatsDumpingInterval = 1000 /* ms */;

int main(int argc, char* argv[]) {
  event::EventLoop loop;

  // Set up periodic stats dumping
  event::Timer statsTimer(kStatsDumpingInterval);
  event::Action statsDumper({statsTimer.didFire()});
  statsDumper.callback = [&statsTimer]() {
    stats::StatsManager::dump(LOG());
    statsTimer.extend(kStatsDumpingInterval);
  };

  bool server = false;
  if (argc > 1 && (strcmp(argv[1], "server") == 0)) {
    server = true;
  }

  LOG() << "Running as " << (server ? "server" : "client") << std::endl;
  CommandCenter center;
  if (server) {
    center.serve(2859);
  } else {
    std::string server = (argc > 1 ? std::string(argv[1]) : "localhost");
    center.connect(server, 2859);
  }

  loop.run();

  return 0;
}
