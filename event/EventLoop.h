#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace event {

using Time = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;

struct NormalTerminationException : public std::exception {
  const char* what() const throw() { return "Normal termination."; }
};

enum class ConditionType {
  Internal,
  IO,
  TimerSignal,
  Signal,
  Computed,
};

class Action;
class Condition;
class BaseCondition;
class ComputedCondition;

class ConditionManager {
public:
  virtual ~ConditionManager() = default;

  virtual void prepareConditions(std::vector<Condition*> const& conditions) = 0;
};

class EventLoopPreparer {
public:
  virtual ~EventLoopPreparer() = default;

  virtual void prepare() = 0;
};

class IOConditionManager;
class SignalConditionManager;
class TimerManager;
class Timer;
class Trigger;

class EventLoop {
public:
  EventLoop();
  ~EventLoop();

  void run();
  void runOnce();
  void addAction(Action* action);
  void removeAction(Action* action);
  void addCondition(Condition* condition);
  bool hasCondition(Condition* condition);
  void removeCondition(Condition* condition);
  void addConditionManager(ConditionManager* manager, ConditionType type);
  void addPreparer(EventLoopPreparer* preparer);

  std::unique_ptr<Action> createAction(const char* name,
                                       std::vector<Condition*> conditions);

  std::unique_ptr<BaseCondition>
  createBaseCondition(ConditionType type = ConditionType::Internal);
  std::unique_ptr<ComputedCondition> createComputedCondition();

  std::unique_ptr<Timer> createTimer();
  std::unique_ptr<Timer> createTimer(Duration timeout);

  IOConditionManager& getIOConditionManager();
  SignalConditionManager& getSignalConditionManager();

  void arm(const char* name, std::vector<event::Condition*> conditions,
           std::function<void(void)> callback);

  void perform(const char* name, std::function<void(void)> callback);

  void performIn(const char* name, event::Duration delay,
                 std::function<void(void)> callback);

private:
  EventLoop(EventLoop const& copy) = delete;
  EventLoop& operator=(EventLoop const& copy) = delete;

  EventLoop(EventLoop const&& move) = delete;
  EventLoop& operator=(EventLoop const&& move) = delete;

  std::set<Action*> actions_;
  std::set<Condition*> conditions_;
  std::vector<std::pair<ConditionType, ConditionManager*>> conditionManagers_;
  std::vector<EventLoopPreparer*> preparers_;

  std::unique_ptr<IOConditionManager> ioConditionManager_;
  std::unique_ptr<SignalConditionManager> signalConditionManager_;
  std::unique_ptr<TimerManager> timerManager_;
  std::unique_ptr<Trigger> triggerManager_;
};
} // namespace event
