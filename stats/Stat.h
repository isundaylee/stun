#pragma once

#include <stats/StatsManager.h>

namespace stats {

template <typename T> class Stat : StatBase {
public:
  Stat(std::string metric, T initialValue, Prefix prefix = Prefix::None)
      : StatBase(metric, prefix), value_(initialValue) {}
  void setName(std::string name) { name_ = name; }

  void accumulate(T const& increment) { value_ += increment; }

private:
  Stat(Stat const& copy) = delete;
  Stat& operator=(Stat const& copy) = delete;

  Stat(Stat&& move) = delete;
  Stat& operator=(Stat&& move) = delete;

  T value_;

  virtual std::string stringValue() override { return std::to_string(value_); }

  friend class StatsManager;
};
}
