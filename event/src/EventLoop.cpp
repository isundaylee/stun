#include "event/EventLoop.h"
#include "event/Action.h"

#include <stdexcept>

namespace event {

EventLoop* EventLoop::instance = nullptr;

EventLoop::EventLoop() :
    actions_(),
    conditions_(),
    conditionManagers_() {
  if (EventLoop::instance != nullptr) {
    throw std::runtime_error("Only 1 EventLoop should be created.");
  }

  EventLoop::instance = this;
}

void EventLoop::addAction(Action* action) {
  actions_.insert(action);
}

void EventLoop::removeAction(Action* action) {
  actions_.erase(action);
}

void EventLoop::addCondition(Condition* condition) {
  conditions_.insert(condition);
}

void EventLoop::removeCondition(Condition* condition) {
  conditions_.erase(condition);
}

void EventLoop::addConditionManager(ConditionManager* manager, ConditionType type) {
  conditionManagers_.emplace_back(type, manager);
}

void EventLoop::run() {
  while (true) {
    // Tell condition managers to prepare conditions they manage. For example, the
    // IO condition manager might use select, poll, or epoll to resolve values for
    // all IO conditions.
    for (auto pair : conditionManagers_) {
      std::vector<Condition*> conditionsToPrepare;
      for (auto condition : conditions_) {
        if (condition->type == pair.first) {
          conditionsToPrepare.push_back(condition);
        }
      }
      pair.second->prepareConditions(conditionsToPrepare);
    }

    // Invoke actions that have all their conditions met
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
