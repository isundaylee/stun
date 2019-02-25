#include "event/SignalCondition.h"
#include "common/Util.h"

#include <csignal>

#include <stdexcept>

namespace event {

SignalCondition::~SignalCondition() {
  switch (type) {
  case SignalType::Int: {
    auto& manager = loop_.getSignalConditionManager();

    auto it =
        std::find(manager.conditions_.begin(), manager.conditions_.end(), this);
    assertTrue(it != manager.conditions_.end(),
               "Cannot find SignalCondition in conditions_ list.");
    manager.conditions_.erase(it);

    switch (type) {
    case SignalType::Int: {
      auto it = manager.sigIntPendingConditions_.find(this);
      assertTrue(
          it != manager.sigIntPendingConditions_.end(),
          "Cannot find SignalCondition in sigIntPendingConditions_ list.");
      manager.sigIntPendingConditions_.erase(it);
      break;
    }
    }
  }
  }
}

SignalConditionManager::Core::Core() {
  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = &SignalConditionManager::Core::handleSignal;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);
}

/* static */ SignalConditionManager::Core&
SignalConditionManager::Core::getInstance() {
  static SignalConditionManager::Core instance;
  return instance;
}

/* static */ void SignalConditionManager::Core::handleSignal(int signal) {
  // TODO: Thread safety??

  if (signal == SIGINT) {
    for (auto manager : SignalConditionManager::Core::getInstance().managers_) {
      manager->sigIntPending = true;
    }
  }
}

SignalConditionManager::SignalConditionManager(EventLoop& loop) : loop_(loop) {
  loop_.addConditionManager(this, ConditionType::Signal);
  SignalConditionManager::Core::getInstance().addManager(this);
}

SignalConditionManager::~SignalConditionManager() {
  SignalConditionManager::Core::getInstance().removeManager(this);
}

std::unique_ptr<SignalCondition>
SignalConditionManager::onSigInt(Condition* pendingCondition) {
  auto condition = std::make_unique<SignalCondition>(loop_, SignalType::Int);
  conditions_.push_back(condition.get());
  sigIntPendingConditions_[condition.get()] = pendingCondition;
  return condition;
}

/* virtual */ void SignalConditionManager::prepareConditions(
    std::vector<Condition*> const& conditions,
    std::vector<Condition*> const& interesting) /* override */ {
  // All signals should be one-shot. Reset them here in the next event loop
  // iteration.
  for (auto cond : conditions) {
    auto signalCond = static_cast<SignalCondition*>(cond);
    signalCond->arm();
  }

  if (sigIntPending) {
    for (auto const& cond : conditions) {
      auto signalCond = static_cast<SignalCondition*>(cond);
      if (signalCond->type == SignalType::Int) {
        signalCond->fire();
      }
    }

    sigIntPending = false;

    auto pendingConditions = std::vector<Condition*>{};
    for (auto pair : sigIntPendingConditions_) {
      pendingConditions.push_back(pair.second);
    }
    terminator_ = loop_.createAction(std::move(pendingConditions));
    terminator_->callback = []() { throw NormalTerminationException(); };
  }
}

}; // namespace event
