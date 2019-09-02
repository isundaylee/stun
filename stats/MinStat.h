#pragma once

#include <stats/StatsManager.h>

#include <limits>

namespace stats {

template <typename T> class MinStat : StatBase {
public:
  MinStat(std::string entity, std::string metric) : StatBase(entity, metric) {}

  void accumulate(T const& value) { min_ = std::min(min_, value); }

private:
  T min_ = std::numeric_limits<T>::max();

  virtual double collect() override { return double(min_); }
};
} // namespace stats
