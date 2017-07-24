#pragma once

#include <event/EventLoop.h>

namespace event {

class Condition {
public:
  Condition(ConditionType type = ConditionType::Base);

  ConditionType type;
  bool value;

private:
  Condition(Condition const& copy) = delete;
  Condition& operator=(Condition const& copy) = delete;

  Condition(Condition const&& move) = delete;
  Condition& operator=(Condition const&& move) = delete;
};

}
