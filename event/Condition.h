#pragma once

#include <event/Callback.h>
#include <event/EventLoop.h>

#include <functional>

namespace event {

class Condition {
public:
  Condition(EventLoop& loop, ConditionType type = ConditionType::Internal)
      : type(type), loop_(loop) {
    loop_.addCondition(this);
  }

  virtual ~Condition() { loop_.removeCondition(this); }

  ConditionType type;

  virtual bool eval() = 0;

private:
  Condition(Condition const& copy) = delete;
  Condition& operator=(Condition const& copy) = delete;

  Condition(Condition const&& move) = delete;
  Condition& operator=(Condition const&& move) = delete;

protected:
  event::EventLoop& loop_;
};

class BaseCondition : public Condition {
public:
  BaseCondition(EventLoop& loop, ConditionType type = ConditionType::Internal)
      : Condition(loop, type), value_(false) {}

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
  ComputedCondition(EventLoop& loop)
      : Condition(loop, ConditionType::Computed) {}

  Callback<bool> expression;

  bool eval() { return expression.invoke(); }

private:
  ComputedCondition(ComputedCondition const& copy) = delete;
  ComputedCondition& operator=(ComputedCondition const& copy) = delete;

  ComputedCondition(ComputedCondition const&& move) = delete;
  ComputedCondition& operator=(ComputedCondition const&& move) = delete;
};
} // namespace event
