#pragma once

#include <event/Condition.h>

#include <signal.h>
#include <stdint.h>
#include <time.h>

#include <chrono>
#include <memory>
#include <queue>

namespace event {

using Time = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;

class Timer {
public:
  Timer();
  Timer(Duration timeout);
  ~Timer();

  Condition* didFire();

  void reset();
  void reset(Duration timeout);
  void extend(Duration timeout);

  static Time getTime();
  static Duration getEpochTimeInMilliseconds();

private:
  Time target_;
  std::unique_ptr<BaseCondition> didFire_;
};
} // namespace event
