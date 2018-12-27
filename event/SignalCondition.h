#pragma once

#include <common/Util.h>

#include <event/Action.h>
#include <event/Condition.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace event {

enum SignalType {
  Int,
};

class SignalCondition : public BaseCondition {
public:
  SignalCondition(EventLoop& loop, SignalType type)
      : BaseCondition(loop, ConditionType::Signal), type(type) {}

  SignalType type;
};

class SignalConditionManager : ConditionManager {
private:
  class Core {
  public:
    static void handleSignal(int signal);

    Core();
    static Core& getInstance();

    void addManager(SignalConditionManager* manager) {
      managers_.push_back(manager);
    }

    void removeManager(SignalConditionManager* manager) {
      auto found = std::find(managers_.begin(), managers_.end(), manager);

      assertTrue(found != managers_.end(),
                 "Attempting to remove an unknown manager from "
                 "SignalConditionManager::Core.");

      managers_.erase(found);
    }

  private:
    std::vector<SignalConditionManager*> managers_;
  };

  EventLoop& loop_;

  bool sigIntPending = false;

  std::vector<Condition*> sigIntPendingConditions_;
  std::vector<SignalCondition*> conditions_;

  std::unique_ptr<Action> terminator_;

public:
  SignalConditionManager(EventLoop& loop);
  ~SignalConditionManager();

  SignalCondition* onSigInt(Condition* pendingCondition);

  virtual void
  prepareConditions(std::vector<Condition*> const& conditions,
                    std::vector<Condition*> const& interesting) override;
};

}; // namespace event
