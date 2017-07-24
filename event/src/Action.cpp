#include "event/Action.h"

namespace event {

Action::Action(std::vector<Condition*> conditions) : conditions_(conditions) {
  EventLoop::getCurrentLoop()->addAction(this);
}

Action::~Action() { EventLoop::getCurrentLoop()->removeAction(this); }

void Action::invoke() { callback.invoke(); }

bool Action::canInvoke() {
  for (auto condition : conditions_) {
    if (!condition->value) {
      return false;
    }
  }

  return true;
}
}
