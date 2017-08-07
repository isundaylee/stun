#pragma once

#include <stats/StatsManager.h>

namespace stats {

template <typename T> class AvgStat : StatBase {
public:
  AvgStat(std::string entity, std::string metric, T initialValue,
          Prefix prefix = Prefix::None)
      : StatBase(entity, metric, prefix), sum_(initialValue), count_(0) {}

  void accumulate(T const& increment) {
    sum_ += increment;
    count_ += 1;
  }

private:
  T sum_;
  T count_;

  virtual std::string collect() override {
    return std::to_string(count_ == 0 ? sum_ : sum_ / count_);
  }
};
}
