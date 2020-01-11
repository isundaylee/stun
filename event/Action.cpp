#include "event/Action.h"

#include <iostream>

namespace event {

Action::Action(EventLoop& loop, const char* actionName,
               std::vector<Condition*> conditions)
    : loop_(loop), actionName_(actionName), conditions_(conditions) {
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
