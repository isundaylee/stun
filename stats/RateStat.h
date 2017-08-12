#pragma once

#include <stats/StatsManager.h>

namespace stats {

class RateStat : StatBase {
public:
  RateStat(std::string entity, std::string metric, double nullValue = 0.0,
           Prefix prefix = Prefix::None)
      : StatBase(entity, metric, prefix), nullValue_(nullValue),
        value_(nullValue) {}

  void accumulate(double const& increment) { value_ += increment; }

private:
  double nullValue_;
  double value_;

  virtual double collect() override {
    auto result = value_;
    value_ = nullValue_;
    return result;
  }
};
}
