#pragma once

#include <stats/StatsManager.h>

namespace stats {

template <typename T> class RateStat : StatBase {
public:
  RateStat(std::string entity, std::string metric, T nullValue,
           Prefix prefix = Prefix::None)
      : StatBase(entity, metric, prefix), nullValue_(nullValue),
        value_(nullValue) {}

  void accumulate(T const& increment) { value_ += increment; }

private:
  T nullValue_;
  T value_;

  virtual std::string collect() override {
    std::string result = std::to_string(value_);
    value_ = nullValue_;
    return result;
  }
};
}
