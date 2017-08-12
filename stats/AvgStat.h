#pragma once

#include <stats/StatsManager.h>

namespace stats {

class AvgStat : StatBase {
public:
  AvgStat(std::string entity, std::string metric, double initialValue = 0.0,
          Prefix prefix = Prefix::None)
      : StatBase(entity, metric, prefix), sum_(initialValue), count_(0) {}

  void accumulate(double const& increment) {
    sum_ += increment;
    count_ += 1;
  }

private:
  double sum_;
  size_t count_;

  virtual double collect() override {
    return count_ == 0 ? sum_ : sum_ / count_;
  }
};
}
