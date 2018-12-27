#pragma once

#include <event/Condition.h>

#include <common/Util.h>

namespace event {

template <typename T> class Promise {
public:
  Promise(EventLoop& loop) : isReady_(loop.createBaseCondition()) {}

  Promise(Promise&& move) = default;
  Promise& operator=(Promise&& move) = default;

  Condition* isReady() const { return isReady_.get(); }

  void fulfill(T value) {
    assertTrue(!isReady_->eval(), "Double-fulfilled Promise.");

    value_ = std::move(value);
    isReady_->fire();
  }

  T consume() {
    assertTrue(
        isReady_->eval(),
        "Attempts to consume an unfulfilled or already consumed Promise.");

    isReady_->arm();
    return std::move(value_);
  }

private:
  std::unique_ptr<BaseCondition> isReady_;
  T value_;

private:
  Promise(Promise const& copy) = delete;
  Promise& operator=(Promise const& copy) = delete;
};
} // namespace event
