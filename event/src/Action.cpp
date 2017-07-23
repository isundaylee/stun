#include "event/Action.h"

namespace event {

Action::Action(std::vector<Condition*> conditions) :
    conditions_(conditions) {
  EventLoop::getCurrentLoop()->addAction(this);
}

void Action::invoke() {
  callback_();
}

bool Action::canInvoke() {
  for (auto condition : conditions_) {
    if (!condition->value) {
      return false;
    }
  }

  return true;
}

}
