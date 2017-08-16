#pragma once

#include <stats/StatsManager.h>

namespace stats {

class AvgStat : StatBase {
public:
  AvgStat(std::string entity, std::string metric) : StatBase(entity, metric) {}

  void accumulate(double const& increment) {
    sum_ += increment;
    count_ += 1;
  }

private:
  double sum_ = 0.0;
  size_t count_ = 0;

  virtual double collect() override {
    return count_ == 0 ? sum_ : sum_ / count_;
  }
};
}
