#pragma once

#include <vector>
#include <set>

namespace event {

enum ConditionType {
  Base,
  IO,
};

class Action;
class Condition;

class ConditionManager {
public:
  virtual void prepareConditions(std::vector<Condition*> const& conditions,
      std::vector<Condition*> const& interesting) = 0;
};

class EventLoop {
public:
  EventLoop();

  void run();
  void addAction(Action* action);
  void removeAction(Action* action);
  void addCondition(Condition* condition);
  void removeCondition(Condition* condition);
  void addConditionManager(ConditionManager* manager, ConditionType type);

  static EventLoop* getCurrentLoop();

private:
  EventLoop(EventLoop const& copy) = delete;
  EventLoop& operator=(EventLoop const& copy) = delete;

  EventLoop(EventLoop const&& move) = delete;
  EventLoop& operator=(EventLoop const&& move) = delete;

  std::set<Action*> actions_;
  std::set<Condition*> conditions_;
  std::vector<std::pair<ConditionType, ConditionManager*>> conditionManagers_;

  static EventLoop* instance;

  void resetExternalConditions();
};

}
