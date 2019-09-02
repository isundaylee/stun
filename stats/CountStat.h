#pragma once

#include <stats/StatsManager.h>

#include <limits>

namespace stats {

class CountStat : StatBase {
public:
  CountStat(std::string entity, std::string metric)
      : StatBase(entity, metric) {}

  void accumulate() { count_++; }

private:
  size_t count_ = 0;

  virtual double collect() override { return double(count_); }
};
} // namespace stats
