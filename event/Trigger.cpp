#include "event/Trigger.h"

#include <common/Util.h>

#include <algorithm>

namespace event {

Trigger::Trigger(event::EventLoop& loop) : loop_(loop) {
  loop_.addPreparer(this);
}

void Trigger::arm(std::vector<event::Condition*> conditions,
                  std::function<void(void)> callback) {
  auto action = loop_.createAction("event::Trigger", conditions);
  auto actionPtr = action.get();

  action->callback = [callback, actionPtr, this]() {
    callback();
    auto it = std::find_if(triggerActions_.begin(), triggerActions_.end(),
                           [actionPtr](std::unique_ptr<Action> const& action) {
                             return action.get() == actionPtr;
                           });
    assertTrue(it != triggerActions_.end(),
               "Cannot find trigger action to remove.");
    triggerActions_.erase(it);
  };

  triggerActions_.push_back(std::move(action));
}

void Trigger::perform(std::function<void(void)> callback) {
  arm({}, [callback]() { callback(); });
}

void Trigger::performIn(event::Duration delay,
                        std::function<void(void)> callback) {
  auto timer =
      std::shared_ptr<event::Timer>(loop_.createTimer(delay).release());

  // We explicitly capture timer by copying here because we need to keep timer
  // alive as long as the trigger.
  arm({timer->didFire()}, [timer, callback]() { callback(); });
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

}; // namespace event
