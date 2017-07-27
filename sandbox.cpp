#include <event/Action.h>
#include <event/Condition.h>
#include <event/EventLoop.h>
#include <event/FIFO.h>
#include <event/IOCondition.h>
#include <event/Timer.h>
#include <event/Trigger.h>

#include <common/Logger.h>

#include <stats/Stat.h>

#include <fcntl.h>
#include <unistd.h>

#include <iostream>

int main(int argc, char* argv[]) {
  common::Logger logger;
  event::EventLoop loop;

  stats::Stat<int> popularity("popularity", 0);
  popularity.setName("client");
  stats::Stat<int> age("age", 0);
  age.setName("client");

  event::Timer timer(0);
  event::Action statsDumper({timer.didFire()});
  statsDumper.callback = [&timer, &logger, &popularity, &age]() {
    stats::StatsManager::dump(logger);
    popularity.accumulate(10);
    age.accumulate(1);
    timer.extend(1000);
  };

  loop.run();

  return 0;
}
