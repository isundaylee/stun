#include "event/Trigger.h"

namespace event {

Trigger* Trigger::instance_ = new Trigger();

Trigger::Trigger() {}

/* static */ void Trigger::arm(std::vector<event::Condition*> conditions,
                               std::function<void(void)> callback) {
  event::Action* action = new event::Action(conditions);
  action->callback = [callback, action]() {
    callback();
    delete action;
  };
}
};
