#pragma once

#include <event/EventLoop.h>

namespace event {

class Condition {
public:
  Condition(ConditionType type = ConditionType::Base)
      : type(type), value_(false) {
    EventLoop::getCurrentLoop()->addCondition(this);
  }

  ~Condition() { EventLoop::getCurrentLoop()->removeCondition(this); }

  ConditionType type;

  bool eval() { return value_; }
  void arm() { value_ = false; }
  void fire() { value_ = true; }
  void set(bool value) { value_ = value; }

private:
  Condition(Condition const& copy) = delete;
  Condition& operator=(Condition const& copy) = delete;

  Condition(Condition const&& move) = delete;
  Condition& operator=(Condition const&& move) = delete;

  bool value_;
};
}
