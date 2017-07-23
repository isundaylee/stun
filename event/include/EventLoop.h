#pragma once

#include <vector>

namespace event {

class Action;

class EventLoop {
public:
  EventLoop();

  void run();
  void addAction(Action* action);

  static EventLoop* getCurrentLoop();

private:
  EventLoop(EventLoop const& copy) = delete;
  EventLoop& operator=(EventLoop const& copy) = delete;

  EventLoop(EventLoop const&& move) = delete;
  EventLoop& operator=(EventLoop const&& move) = delete;

  std::vector<Action*> actions_;

  static EventLoop* instance;
};

}
