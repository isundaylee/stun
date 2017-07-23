#include "event/EventLoop.h"
#include "event/Action.h"

#include <stdexcept>

namespace event {

EventLoop* EventLoop::instance = nullptr;

EventLoop::EventLoop() :
    actions_() {
  if (EventLoop::instance != nullptr) {
    throw std::runtime_error("Only 1 EventLoop should be created.");
  }

  EventLoop::instance = this;
}

void EventLoop::addAction(Action* action) {
  actions_.push_back(action);
}

void EventLoop::run() {
  while (true) {
    for (auto action : actions_) {
      if (action->canInvoke()) {
        action->invoke();
      }
    }
  }
}

EventLoop* EventLoop::getCurrentLoop() {
  if (EventLoop::instance == nullptr) {
    throw std::runtime_error("No current EventLoop exists.");
  }

  return EventLoop::instance;
}

}
