#pragma once

#include <event/Action.h>
#include <event/Condition.h>

#include <functional>
#include <memory>
#include <vector>

namespace event {

class Trigger {
public:
  static void arm(std::vector<event::Condition*> conditions,
                  std::function<void(void)> callback);

private:
  Trigger();

  static Trigger* instance_;
};
}
