#pragma once

#include <event/Callback.h>
#include <event/Condition.h>
#include <event/EventLoop.h>

#include <functional>
#include <vector>

namespace event {

class Action {
public:
  Action(EventLoop& loop, std::vector<Condition*> conditions);
  Action(std::vector<Condition*> conditions);
  ~Action();

  friend class EventLoop;

  void invoke();
  bool canInvoke() const;
  bool isDead() const;

  Callback<void> callback;

private:
  Action(Action const& copy) = delete;
  Action& operator=(Action const& copy) = delete;

  Action(Action const&& move) = delete;
  Action& operator=(Action const&& move) = delete;

  std::vector<Condition*> conditions_;
};
} // namespace event
