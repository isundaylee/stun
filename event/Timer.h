#pragma once

#include <common/Util.h>

#include <event/Condition.h>

#include <stats/MinStat.h>

#include <signal.h>
#include <stdint.h>
#include <time.h>

#include <algorithm>
#include <memory>
#include <queue>

namespace event {

class TimerManager : ConditionManager {
public:
  friend class Timer;

  TimerManager(EventLoop& loop);
  ~TimerManager();

  void setTimeout(Time target, BaseCondition* condition);
  void removeTimeout(BaseCondition* condition);

  virtual void prepareConditions(std::vector<Condition*> const& conditions,
                                 std::vector<Condition*> const& interesting);

private:
  typedef std::pair<Time, BaseCondition*> TimeoutTrigger;

  class Core {
  public:
    Core();
    static Core& getInstance();

    static void handleSignal(int sig, siginfo_t* si, void* uc);

    void addManager(TimerManager* manager);
    void removeManager(TimerManager* manager);
    void requestTimeout(Time target);

    void maskSignal();
    void unmaskSignal();

  private:
    std::vector<TimerManager*> managers_;

    bool pending_ = false;
    Time masterClock_ = std::numeric_limits<Time>::min();
    Time currentTarget_;

    stats::MinStat<uint64_t> statMinRemainingTimeUSec_;
  };

  void handleTimeout(Time target);

  EventLoop& loop_;

  Time clock_ = std::chrono::steady_clock::now();
  std::vector<TimeoutTrigger> targets_;

  void updateTimer();
};

class Timer {
public:
  Timer(TimerManager& manager);
  Timer(TimerManager& manager, Duration timeout);
  ~Timer();

  Condition* didFire();

  void reset();
  void reset(Duration timeout);
  void extend(Duration timeout);

  static Time getTime();
  static Duration getEpochTimeInMilliseconds();

private:
  TimerManager& manager_;

  Time target_;
  std::unique_ptr<BaseCondition> didFire_;
};
} // namespace event
