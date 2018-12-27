#include "event/EventLoop.h"

#include <event/Action.h>
#include <event/IOCondition.h>
#include <event/SignalCondition.h>
#include <event/Timer.h>
#include <event/Trigger.h>

#include <common/Util.h>

#include <iostream>
#include <stdexcept>

namespace event {

EventLoop::EventLoop()
    : actions_(), conditions_(), conditionManagers_(),
      ioConditionManager_(new IOConditionManager(*this)),
      signalConditionManager_(new SignalConditionManager(*this)),
      timerManager_(new TimerManager(*this)),
      triggerManager_(new Trigger(*this)) {}

EventLoop::~EventLoop() = default;

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

void EventLoop::addPreparer(EventLoopPreparer* preparer) {
  preparers_.push_back(preparer);
}

std::unique_ptr<Action>
EventLoop::createAction(std::vector<Condition*> conditions) {
  return std::make_unique<Action>(*this, conditions);
}

std::unique_ptr<BaseCondition> EventLoop::createBaseCondition(
    ConditionType type /* = ConditionType::Internal */) {
  return std::make_unique<BaseCondition>(*this, type);
}

std::unique_ptr<ComputedCondition> EventLoop::createComputedCondition() {
  return std::make_unique<ComputedCondition>(*this);
}

std::unique_ptr<Timer> EventLoop::createTimer() {
  return std::make_unique<Timer>(*timerManager_);
}
std::unique_ptr<Timer> EventLoop::createTimer(Duration timeout) {
  return std::make_unique<Timer>(*timerManager_, timeout);
}

IOConditionManager& EventLoop::getIOConditionManager() {
  return *ioConditionManager_.get();
}

SignalConditionManager& EventLoop::getSignalConditionManager() {
  return *signalConditionManager_.get();
}

void EventLoop::arm(std::vector<event::Condition*> conditions,
                    std::function<void(void)> callback) {
  return triggerManager_->arm(conditions, callback);
}

void EventLoop::perform(std::function<void(void)> callback) {
  return triggerManager_->perform(callback);
}

void EventLoop::performIn(event::Duration delay,
                          std::function<void(void)> callback) {
  return triggerManager_->performIn(delay, callback);
}

void EventLoop::run() {
  try {
    while (true) {
      runOnce();
    }
  } catch (NormalTerminationException) {
    // Nothing
  }
}

void EventLoop::runOnce() {
  // First we run all the preparers
  //
  // They must be run before the subsequent dead action purging, as some types
  // of events (e.g. Trigger) are allowed to be dead, and the Trigger manager
  // itself is supposed to purge those.
  for (auto preparer : preparers_) {
    preparer->prepare();
  }

  // Then we purge the dead actions (i.e. actions that refer to at least one
  // condition that doesn't exist anymore)
  size_t purged = 0;
  for (auto it = actions_.begin(); it != actions_.end();) {
    if ((*it)->isDead()) {
      purged++;
      it = actions_.erase(it);
    } else {
      it++;
    }
  }

  if (purged > 0) {
    LOG_E("Event") << "Purged " << std::to_string(purged)
                   << " dead actions. This should ideally never happen."
                   << std::endl;
  }

  // Tell condition managers to prepare conditions they manage. For example,
  // the IO condition manager might use select, poll, or epoll to resolve
  // values for all IO conditions.

  // Conditions are divided into two types: internal and external. External
  // conditions are those related to external I/O. Internal conditions are
  // those changed exclusively as a result of an Action.

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

        if (condition->type == ConditionType::Internal && !condition->eval()) {
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
  std::set<Action*> toInvoke;

  for (auto action : actions_) {
    if (action->canInvoke()) {
      toInvoke.insert(action);
    }
  }

  for (auto actionToInvoke : toInvoke) {
    // Invoking some previous action in this round could have caused this
    // action to be removed already. So we need to recheck.
    // Also firing an action could invalidate other actions. So we need to
    // recheck that as well.
    if (actions_.find(actionToInvoke) != actions_.end() &&
        actionToInvoke->canInvoke()) {
      actionToInvoke->invoke();
    }
  }
}

} // namespace event
