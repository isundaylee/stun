#pragma once

#include <event/Callback.h>
#include <event/EventLoop.h>

#include <functional>

namespace event {

class Condition {
public:
  Condition(ConditionType type = ConditionType::Internal) : type(type) {
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
  BaseCondition(ConditionType type = ConditionType::Internal)
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

class ComputedCondition : public Condition {
public:
  ComputedCondition() : Condition(ConditionType::Internal) {}

  Callback<bool> expression;

  bool eval() { return expression.invoke(); }

private:
  ComputedCondition(ComputedCondition const& copy) = delete;
  ComputedCondition& operator=(ComputedCondition const& copy) = delete;

  ComputedCondition(ComputedCondition const&& move) = delete;
  ComputedCondition& operator=(ComputedCondition const&& move) = delete;
};
}
