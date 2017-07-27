#pragma once

#include <stats/StatsManager.h>

namespace stats {

template <typename T> class RateStat : StatBase {
public:
  RateStat(std::string metric, T nullValue, Prefix prefix = Prefix::None)
      : StatBase(metric, prefix), nullValue_(nullValue), value_(nullValue) {}
  void setName(std::string name) { name_ = name; }

  void accumulate(T const& increment) { value_ += increment; }

private:
  RateStat(RateStat const& copy) = delete;
  RateStat& operator=(RateStat const& copy) = delete;

  RateStat(RateStat&& move) = delete;
  RateStat& operator=(RateStat&& move) = delete;

  T nullValue_;
  T value_;

  virtual std::string stringValue() override {
    std::string result = std::to_string(value_);
    value_ = nullValue_;
    return result;
  }

  friend class StatsManager;
};
}
