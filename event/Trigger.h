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
  Trigger(event::EventLoop& loop);

  // Arm a callback to be called when the given conditions fire. The trigger
  // would self-destruct once the conditions fire. It will also self-destruct
  // if some of the conditions it depends on is already removed from the event
  // loop.
  void arm(const char* name, std::vector<event::Condition*> conditions,
           std::function<void(void)> callback);

  void perform(const char* name, std::function<void(void)> callback);

  void performIn(const char* name, event::Duration delay,
                 std::function<void(void)> callback);

  virtual void prepare() override;

private:
  event::EventLoop& loop_;

  std::vector<std::unique_ptr<Action>> triggerActions_;
};
} // namespace event
