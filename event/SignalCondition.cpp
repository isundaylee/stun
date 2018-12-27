#include "event/SignalCondition.h"
#include "common/Util.h"

#include <csignal>

#include <stdexcept>

namespace event {

SignalConditionManager::SignalConditionManager() {
  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = &SignalConditionManager::handleSignal;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);

  event::EventLoop::getCurrentLoop().addConditionManager(this,
                                                         ConditionType::Signal);
}

/* static */ SignalConditionManager& SignalConditionManager::getInstance() {
  static SignalConditionManager instance;
  return instance;
}

/* static */ void SignalConditionManager::handleSignal(int signal) {
  if (signal == SIGINT) {
    SignalConditionManager::getInstance().sigIntPending_ = true;
  }
}

SignalCondition* SignalConditionManager::onSigInt(Condition* pendingCondition) {
  SignalCondition* condition = new SignalCondition(SignalType::Int);
  SignalConditionManager::getInstance().conditions_.push_back(condition);
  SignalConditionManager::getInstance().sigIntPendingConditions_.push_back(
      pendingCondition);
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

  if (sigIntPending_) {
    for (auto const& cond : conditions) {
      auto signalCond = static_cast<SignalCondition*>(cond);
      if (signalCond->type == SignalType::Int) {
        signalCond->fire();
      }
    }

    sigIntPending_ = false;

    auto sigIntPendingConditions =
        SignalConditionManager::getInstance().sigIntPendingConditions_;
    SignalConditionManager::getInstance().terminator_ =
        event::EventLoop::getCurrentLoop().createAction(
            sigIntPendingConditions_);
    SignalConditionManager::getInstance().terminator_->callback = []() {
      throw NormalTerminationException();
    };
  }
}

}; // namespace event
