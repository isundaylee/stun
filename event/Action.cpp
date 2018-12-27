#include "event/Action.h"

#include <iostream>

namespace event {

Action::Action(EventLoop& loop, std::vector<Condition*> conditions)
    : loop_(loop), conditions_(conditions) {
  loop.addAction(this);
}

Action::~Action() { loop_.removeAction(this); }

void Action::invoke() { callback.invoke(); }

bool Action::canInvoke() const {
  for (auto condition : conditions_) {
    if (!loop_.hasCondition(condition) || !condition->eval()) {
      return false;
    }
  }

  return true;
}

bool Action::isDead() const {
  for (auto condition : conditions_) {
    if (!loop_.hasCondition(condition)) {
      return true;
    }
  }

  return false;
}
} // namespace event
