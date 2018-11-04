#pragma once

#include <event/Action.h>
#include <event/Condition.h>

#include <memory>
#include <vector>
#include <stdexcept>

namespace event {

enum SignalType {
  Int,
};

class SignalCondition : public BaseCondition {
public:
  SignalCondition(SignalType type)
      : BaseCondition(ConditionType::Signal), type(type) {}

  SignalType type;
};

class SignalConditionManager : ConditionManager {
private:
  static void handleSignal(int signal);

  std::vector<Condition*> sigIntPendingConditions_;
  std::vector<SignalCondition*> conditions_;

  std::unique_ptr<Action> terminator_;

  bool sigIntPending_ = false;

public:
  SignalConditionManager();

  static SignalConditionManager& getInstance();

  static SignalCondition* onSigInt(Condition* pendingCondition);

  virtual void
  prepareConditions(std::vector<Condition*> const& conditions,
                    std::vector<Condition*> const& interesting) override;
};

}; // namespace event
