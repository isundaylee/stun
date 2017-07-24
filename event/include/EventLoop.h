#pragma once

#include <vector>

namespace event {

enum ConditionType {
  Base,
  IO,
};

class Action;
class Condition;

class ConditionManager {
public:
  virtual void prepareConditions(std::vector<Condition*> const& conditions) = 0;
};

class EventLoop {
public:
  EventLoop();

  void run();
  void addAction(Action* action);
  void addCondition(Condition* condition);
  void addConditionManager(ConditionManager* manager, ConditionType type);

  static EventLoop* getCurrentLoop();

private:
  EventLoop(EventLoop const& copy) = delete;
  EventLoop& operator=(EventLoop const& copy) = delete;

  EventLoop(EventLoop const&& move) = delete;
  EventLoop& operator=(EventLoop const&& move) = delete;

  std::vector<Action*> actions_;
  std::vector<Condition*> conditions_;
  std::vector<std::pair<ConditionType, ConditionManager*>> conditionManagers_;

  static EventLoop* instance;
};

}
