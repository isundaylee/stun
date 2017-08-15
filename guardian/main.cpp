#include <guardian/Monitor.h>

#include <common/Logger.h>
#include <common/Util.h>
#include <event/EventLoop.h>

#include <iostream>

using namespace guardian;
using namespace std::chrono_literals;

static const event::Duration kStatsDumpInterval = 1s;

int main(int argc, char* argv[]) {
  event::EventLoop loop;
  Monitor monitor;

  std::unique_ptr<event::Timer> statsTimer;
  std::unique_ptr<event::Action> statsDumper;

  // Sets up stats collection
  statsTimer.reset(new event::Timer{kStatsDumpInterval});
  statsDumper.reset(new event::Action{{statsTimer->didFire()}});
  statsDumper->callback = [&statsTimer]() {
    stats::StatsManager::collect();
    statsTimer->extend(kStatsDumpInterval);
  };

  stats::StatsManager::subscribe([](auto const& data) {
    stats::StatsManager::dump(LOG_I("Stats"), data);
  });

  loop.run();

  return 0;
}
