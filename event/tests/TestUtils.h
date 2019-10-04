#pragma once

#include <common/Util.h>

#include <unistd.h>

#include <chrono>

#include <fstream>
#include <iostream>
#include <string>

class TestTrigger {
public:
  TestTrigger() {}

  TestTrigger(TestTrigger const& rhs) = delete;
  TestTrigger& operator=(TestTrigger const& rhs) = delete;

  void fire() { fired_ = true; }

  bool hasFired() const { return fired_; }

private:
  bool fired_ = false;
};

// Returns the total CPU time (user + kernel mode) the current process has
// spent.
#if TARGET_LINUX
inline std::chrono::milliseconds getCPUTime() {
  auto clock_tick = sysconf(_SC_CLK_TCK);

  auto f = std::ifstream{"/proc/self/stat"};
  auto token = std::string{};

  // Skip the first 13 fields
  for (auto i = 0; i < 13; i++) {
    f >> token;
  }

  f >> token;
  auto utime = atoi(token.c_str());

  f >> token;
  auto stime = atoi(token.c_str());

  return std::chrono::milliseconds{1000 * (utime + stime) / clock_tick};
}
#endif