#pragma once

#include <stats/StatsManager.h>

namespace stats {

template <typename T> class Stat : StatBase {
public:
  Stat(std::string metric, T initialValue)
      : StatBase(metric), value_(initialValue) {}
  void setName(std::string name) { name_ = name; }

  void accumulate(T const& increment) { value_ += increment; }

private:
  T value_;

  virtual std::string stringValue() override { return std::to_string(value_); }

  friend class StatsManager;
};
}
