#pragma once

#include <event/Callback.h>
#include <event/Condition.h>
#include <event/EventLoop.h>

#include <functional>
#include <vector>

namespace event {

class Action {
public:
  Action(std::vector<Condition*> conditions);
  ~Action();

  friend class EventLoop;

  void invoke();
  bool canInvoke();

  Callback callback;

private:
  std::vector<Condition*> conditions_;
};
}
