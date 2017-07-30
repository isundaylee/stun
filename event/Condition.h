#pragma once

#include <event/EventLoop.h>

namespace event {

class Condition {
public:
  Condition(ConditionType type = ConditionType::Base) : type(type) {
    EventLoop::getCurrentLoop()->addCondition(this);
  }

  virtual ~Condition() { EventLoop::getCurrentLoop()->removeCondition(this); }

  ConditionType type;

  virtual bool eval() = 0;

private:
  Condition(Condition const& copy) = delete;
  Condition& operator=(Condition const& copy) = delete;

  Condition(Condition const&& move) = delete;
  Condition& operator=(Condition const&& move) = delete;
};

class BaseCondition : public Condition {
public:
  BaseCondition(ConditionType type = ConditionType::Base)
      : Condition(type), value_(false) {}

  bool eval() { return value_; }
  void arm() { value_ = false; }
  void fire() { value_ = true; }
  void set(bool value) { value_ = value; }

private:
  BaseCondition(BaseCondition const& copy) = delete;
  BaseCondition& operator=(BaseCondition const& copy) = delete;

  BaseCondition(BaseCondition const&& move) = delete;
  BaseCondition& operator=(BaseCondition const&& move) = delete;

  bool value_;
};
}
