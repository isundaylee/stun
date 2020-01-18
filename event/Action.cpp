#include "event/Action.h"

#include <common/Logger.h>

#include <iostream>

namespace event {

Action::Action(EventLoop& loop, const char* actionName,
               std::vector<Condition*> conditions)
    : loop_(loop), actionName_(actionName), conditions_(conditions) {
  loop.addAction(this);
  LOG_VV("Action") << "Added:    " << actionName_ << std::endl;
}

Action::~Action() {
  loop_.removeAction(this);
  LOG_VV("Action") << "Removed:  " << actionName_ << std::endl;
}

void Action::invoke() {
  // We need to save a copy since callback.invoke() might destruct this Action
  // (e.g. for Trigger).
  const char* actionNameCopy = actionName_;
  callback.invoke();
  LOG_VV("Action") << "Invoked:  " << actionNameCopy << std::endl;
}

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
