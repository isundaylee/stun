#include <chrono>
#include <iostream>
#include <random>
#include <utility>

#include <event/Action.h>
#include <event/EventLoop.h>
#include <event/Timer.h>

#include <stats/StatsManager.h>

constexpr size_t numActions = 2;

auto getRandomDuration() {
  static std::random_device dev;
  static std::mt19937 rng{dev()};
  static std::uniform_int_distribution<> dist(1, 5);

  return std::chrono::milliseconds{dist(rng)};
}

int main(int argc, char* argv[]) {
  event::EventLoop loop{};

  auto timers = std::vector<std::unique_ptr<event::Timer>>{};
  auto actions = std::vector<std::unique_ptr<event::Action>>{};

  for (auto i = 0; i < numActions; i++) {
    timers.push_back(loop.createTimer());
    timers.back()->reset(getRandomDuration());

    auto timer = timers.back().get();

    actions.push_back(loop.createAction({timers.back()->didFire()}));
    actions.back()->callback = [timer] {
      // std::cout << std::chrono::high_resolution_clock::now()
      //                  .time_since_epoch()
      //                  .count()
      //           << std::endl;
      // std::cout << "." << std::flush;
      timer->reset(getRandomDuration());
    };
  }

  stats::StatsManager::subscribe([](auto data) {
    for (auto const& [k, v] : data) {
      std::cout << k.first << " " << k.second << " " << v << std::endl;
    }
  });

  auto dumpTimer = loop.createTimer();
  dumpTimer->reset(std::chrono::seconds{1});

  auto dumpAction = loop.createAction({dumpTimer->didFire()});
  dumpAction->callback = [&dumpTimer] {
    stats::StatsManager::collect();
    dumpTimer->extend(std::chrono::seconds{1});
  };

  loop.run();

  return 0;
}
