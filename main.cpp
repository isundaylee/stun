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
    // center.connect("54.174.137.123", 2859);
    // center.connect("127.0.0.1", 2859);
    center.connect("tencent.ljh.me", 2859);
  }

  loop.run();

  return 0;
}
