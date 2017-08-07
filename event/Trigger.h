#pragma once

#include <event/Action.h>
#include <event/Condition.h>
#include <event/Timer.h>

#include <functional>
#include <memory>
#include <vector>

namespace event {

class Trigger : EventLoopPreparer {
public:
  // Arm a callback to be called when the given conditions fire. The trigger
  // would self-destruct once the conditions fire. It will also self-destruct
  // if some of the conditions it depends on is already removed from the event
  // loop.
  static void arm(std::vector<event::Condition*> conditions,
                  std::function<void(void)> callback);

  static void performIn(event::Duration delay,
                        std::function<void(void)> callback);

  virtual void prepare() override;

private:
  Trigger();
  static Trigger& getInstance();

  std::vector<std::unique_ptr<Action>> triggerActions_;
};
}
