#pragma once

#include <event/Action.h>
#include <event/Condition.h>

#include <functional>
#include <vector>
#include <memory>

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
