#include "event/SignalCondition.h"
#include "common/Util.h"

#include <csignal>

#include <stdexcept>

namespace event {

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

SignalCondition* SignalConditionManager::onSigInt(Condition* pendingCondition) {
  SignalCondition* condition = new SignalCondition(loop_, SignalType::Int);
  conditions_.push_back(condition);
  sigIntPendingConditions_.push_back(pendingCondition);
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

    terminator_ = loop_.createAction(sigIntPendingConditions_);
    terminator_->callback = []() { throw NormalTerminationException(); };
  }
}

}; // namespace event
