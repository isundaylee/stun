#pragma once

#include <stats/StatsManager.h>

namespace stats {

class RatioStat : StatBase {
public:
  RatioStat(std::string entity, std::string metric)
      : StatBase(entity, metric) {}

  void accumulate(double numerator, double denominator) {
    numerator_ += numerator;
    denominator_ += denominator;
  }

private:
  double numerator_ = 0.0, denominator_ = 0.0;

  virtual double collect() override { return numerator_ / denominator_; }
};
}
