#include "event/Trigger.h"

#include <common/Util.h>

#include <algorithm>

namespace event {

Trigger::Trigger() { EventLoop::getCurrentLoop()->addPreparer(this); }

/* static */ void Trigger::arm(std::vector<event::Condition*> conditions,
                               std::function<void(void)> callback) {
  auto& instance = Trigger::getInstance();
  auto action = std::make_unique<Action>(conditions);
  auto actionPtr = action.get();

  action->callback = [callback, actionPtr, &instance]() {
    callback();
    auto it = std::find_if(instance.triggerActions_.begin(),
                           instance.triggerActions_.end(),
                           [actionPtr](std::unique_ptr<Action> const& action) {
                             return action.get() == actionPtr;
                           });
    assertTrue(it != instance.triggerActions_.end(),
               "Cannot find trigger action to remove.");
    instance.triggerActions_.erase(it);
  };

  instance.triggerActions_.push_back(std::move(action));
}

/* virtual */ void Trigger::prepare() /*override */ {
  for (auto it = triggerActions_.begin(); it != triggerActions_.end();) {
    if ((*it)->isDead()) {
      it = triggerActions_.erase(it);
    } else {
      it++;
    }
  }
}

/* static */ Trigger& Trigger::getInstance() {
  static Trigger instance;
  return instance;
}
};
