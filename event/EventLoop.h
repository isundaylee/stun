#pragma once

#include <memory>
#include <set>
#include <vector>

namespace event {

struct NormalTerminationException : public std::exception {
  const char* what() const throw() { return "Normal termination."; }
};

enum class ConditionType {
  Internal,
  IO,
  TimerSignal,
  Signal,
};

class Action;
class Condition;
class BaseCondition;
class ComputedCondition;

class ConditionManager {
public:
  virtual void
  prepareConditions(std::vector<Condition*> const& conditions,
                    std::vector<Condition*> const& interesting) = 0;
};

class EventLoopPreparer {
public:
  virtual void prepare() = 0;
};

class IOConditionManager;

class EventLoop {
public:
  void run();
  void runOnce();
  void addAction(Action* action);
  void removeAction(Action* action);
  void addCondition(Condition* condition);
  bool hasCondition(Condition* condition);
  void removeCondition(Condition* condition);
  void addConditionManager(ConditionManager* manager, ConditionType type);
  void addPreparer(EventLoopPreparer* preparer);

  std::unique_ptr<Action> createAction(std::vector<Condition*> conditions);

  std::unique_ptr<BaseCondition>
  createBaseCondition(ConditionType type = ConditionType::Internal);
  std::unique_ptr<ComputedCondition> createComputedCondition();

  IOConditionManager& getIOConditionManager();

  static EventLoop& getCurrentLoop();

private:
  EventLoop();
  ~EventLoop();

  EventLoop(EventLoop const& copy) = delete;
  EventLoop& operator=(EventLoop const& copy) = delete;

  EventLoop(EventLoop const&& move) = delete;
  EventLoop& operator=(EventLoop const&& move) = delete;

  std::set<Action*> actions_;
  std::set<Condition*> conditions_;
  std::vector<std::pair<ConditionType, ConditionManager*>> conditionManagers_;
  std::vector<EventLoopPreparer*> preparers_;

  std::unique_ptr<IOConditionManager> ioConditionManager_;
};
} // namespace event
