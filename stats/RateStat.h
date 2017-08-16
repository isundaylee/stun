#pragma once

#include <stats/StatsManager.h>

namespace stats {

class RateStat : StatBase {
public:
  RateStat(std::string entity, std::string metric) : StatBase(entity, metric) {}

  void accumulate(double const& increment) { value_ += increment; }

private:
  double value_ = 0.0;

  virtual double collect() override {
    auto result = value_;
    value_ = 0.0;
    return result;
  }
};
}
