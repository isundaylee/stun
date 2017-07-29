#include "event/EventLoop.h"
#include "event/Action.h"
#include "event/IOCondition.h"

#include <iostream>
#include <stdexcept>

namespace event {

EventLoop* EventLoop::instance = nullptr;

EventLoop::EventLoop() : actions_(), conditions_(), conditionManagers_() {
  if (EventLoop::instance != nullptr) {
    throw std::runtime_error("Only 1 EventLoop should be created.");
  }

  EventLoop::instance = this;

  // This is needed to force IOConditionManager to initialize and attach its
  // preparer, even in the case that the program doesn't actually use IO.
  // Otherwise we have no blocking operation on the event loop, and the CPU
  // usage is going to skyrocket
  IOConditionManager::canRead(0);
}

void EventLoop::addAction(Action* action) { actions_.insert(action); }

void EventLoop::removeAction(Action* action) { actions_.erase(action); }

void EventLoop::addCondition(Condition* condition) {
  conditions_.insert(condition);
}

bool EventLoop::hasCondition(Condition* condition) {
  return (conditions_.find(condition) != conditions_.end());
}

void EventLoop::removeCondition(Condition* condition) {
  conditions_.erase(condition);
}

void EventLoop::addConditionManager(ConditionManager* manager,
                                    ConditionType type) {
  conditionManagers_.emplace_back(type, manager);
}

void EventLoop::run() {
  while (true) {
    // Tell condition managers to prepare conditions they manage. For example,
    // the
    // IO condition manager might use select, poll, or epoll to resolve values
    // for
    // all IO conditions.

    // Conditions are divided into two types: internal and external. External
    // conditions are those related to external I/O. Internal conditions are
    // those
    // changed exclusively as a result of an Action.

    // First we need to find all "interesting" external conditions. An external
    // condition is interesting if it could potentially unblock at least one
    // action.
    // On the contrary, if an action currently has an internal condition unmet,
    // it will not be unblocked by any movement of its external conditions.

    // We then pass the set of all type-matching conditions, and the set of all
    // "interesting" conditions to the condition manager.

    for (auto pair : conditionManagers_) {
      // Find all conditions of the given type
      std::vector<Condition*> conditionsWithType;
      for (auto condition : conditions_) {
        if (condition->type == pair.first) {
          conditionsWithType.push_back(condition);
        }
      }

      // Find all "interesting" conditions of the type
      std::set<Condition*> interesting;
      for (auto action : actions_) {
        bool eligible = true;
        for (auto condition : action->conditions_) {
          if (!hasCondition(condition)) {
            continue;
          }

          if (condition->type == ConditionType::Base && !(*condition)) {
            eligible = false;
            break;
          }
        }
        if (eligible) {
          for (auto condition : action->conditions_) {
            if (!hasCondition(condition)) {
              continue;
            }

            if (condition->type == pair.first) {
              interesting.insert(condition);
            }
          }
        }
      }
      pair.second->prepareConditions(
          conditionsWithType,
          std::vector<Condition*>(interesting.begin(), interesting.end()));
    }

    // Invoke actions that have all their conditions met
    while (true) {
      bool stablized = true;
      for (auto action : actions_) {
        if (action->canInvoke()) {
          stablized = false;
          action->invoke();
          // Invoking the current action could have caused some of the actions
          // to be removed from actions_. Therefore we break and start looking
          // again.
          break;
        }
      }
      resetExternalConditions();
      if (stablized) {
        break;
      }
    }
  }
}

void EventLoop::resetExternalConditions() {
  for (auto condition : conditions_) {
    if (condition->type != ConditionType::Base) {
      condition->arm();
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
