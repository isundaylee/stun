#include "event/Action.h"

#include <iostream>

namespace event {

Action::Action(std::vector<Condition*> conditions) : conditions_(conditions) {
  EventLoop::getCurrentLoop()->addAction(this);
}

Action::~Action() { EventLoop::getCurrentLoop()->removeAction(this); }

void Action::invoke() {
  if (!callback.invoke()) {
    // The callback is empty
    throw std::runtime_error(
        "Action without a callback added to the event loop.");
  }
}

bool Action::canInvoke() {
  for (auto condition : conditions_) {
    if (!*condition) {
      return false;
    }
  }

  return true;
}
}
