#include "event/Timer.h"

#include <common/Util.h>

#include <sys/time.h>

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

namespace event {

using namespace std::chrono_literals;

class TimerManager : ConditionManager {
public:
  static TimerManager& getInstance();
  static void setTimeout(Time target, BaseCondition* condition);
  static void removeTimeout(BaseCondition* condition);

  virtual void prepareConditions(std::vector<Condition*> const& conditions,
                                 std::vector<Condition*> const& interesting);

private:
  TimerManager();

  typedef std::pair<Time, BaseCondition*> TimeoutTrigger;

  std::vector<TimeoutTrigger> targets_;
  std::unique_ptr<Time> currentTarget_;
  bool currentTargetFired_ = false;

  static void handleSignal(int sig, siginfo_t* si, void* uc);

  void sortTargets();
  void updateTimer(Time now);
  void fireUntilTarget();
};

#if TARGET_LINUX
const int kTimerManagerSignal = SIGRTMIN;
#endif

TimerManager::TimerManager() {
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &TimerManager::handleSignal;
  sigemptyset(&sa.sa_mask);

  EventLoop::getCurrentLoop().addConditionManager(this, Signal);

  // Bind the timer singal handler
  if (sigaction(SIGALRM, &sa, NULL) < 0) {
    throw std::runtime_error("Cannot bind timer signal handler.");
  }
}

TimerManager& TimerManager::getInstance() {
  static TimerManager instance;
  return instance;
}

/* virtual */ void
TimerManager::prepareConditions(std::vector<Condition*> const& conditions,
                                std::vector<Condition*> const& interesting) {
  if (!currentTargetFired_) {
    return;
  }

  Time now = Timer::getTime();
  Time target = std::max(now, *currentTarget_);
  while (!targets_.empty() && targets_.back().first <= target) {
    TimeoutTrigger fired = targets_.back();
    targets_.pop_back();
    fired.second->fire();
  }
  currentTarget_.reset();
  currentTargetFired_ = false;
  updateTimer(now);
}

void TimerManager::sortTargets() {
  std::sort(targets_.begin(), targets_.end(), std::greater<TimeoutTrigger>());
}

/* static */ void TimerManager::setTimeout(Time target,
                                           BaseCondition* condition) {
  auto existing =
      std::find_if(getInstance().targets_.begin(), getInstance().targets_.end(),
                   [condition](TimeoutTrigger const& trigger) {
                     return trigger.second == condition;
                   });

  if (existing == getInstance().targets_.end()) {
    getInstance().targets_.emplace_back(target, condition);
  } else {
    existing->first = target;
  }

  Time now = Timer::getTime();
  getInstance().sortTargets();
  getInstance().updateTimer(now);
}

/* static */ void TimerManager::removeTimeout(BaseCondition* condition) {
  auto existing =
      std::find_if(getInstance().targets_.begin(), getInstance().targets_.end(),
                   [condition](TimeoutTrigger const& trigger) {
                     return trigger.second == condition;
                   });

  if (existing != getInstance().targets_.end()) {
    getInstance().targets_.erase(existing);
  }
}

Time Timer::getTime() { return std::chrono::steady_clock::now(); }

Duration Timer::getEpochTimeInMilliseconds() {
  return std::chrono::duration_cast<Duration>(getTime().time_since_epoch());
}

void TimerManager::handleSignal(int sig, siginfo_t* si, void* uc) {
  getInstance().currentTargetFired_ = true;
}

void TimerManager::updateTimer(Time now) {
  if (targets_.empty()) {
    return;
  }

  if (!!currentTarget_ && targets_.back().first >= *currentTarget_) {
    // The current timer will still fire before our earliest event.
    // We don't have to change a thing.
    return;
  }

  Duration timeout =
      std::max(std::chrono::milliseconds(1),
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   targets_.back().first - now));

  struct itimerval its;
  its.it_value.tv_sec = timeout / 1s;
  its.it_value.tv_usec =
      std::chrono::duration_cast<std::chrono::microseconds>(timeout % 1s)
          .count();
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_usec = 0;

  int ret = setitimer(ITIMER_REAL, &its, NULL);
  assertTrue(ret == 0, "Cannot set timer.");

  currentTarget_.reset(new Time(now + timeout));
}

Timer::Timer() : didFire_(new BaseCondition()) {}

Timer::Timer(Duration timeout)
    : didFire_(new BaseCondition(ConditionType::Signal)) {
  reset(timeout);
}

Timer::~Timer() { TimerManager::removeTimeout(didFire_.get()); }

Condition* Timer::didFire() { return didFire_.get(); }

void Timer::reset() { didFire_.get()->arm(); }

void Timer::reset(Duration timeout) {
  reset();
  Time now = getTime();
  target_ = now + timeout;
  TimerManager::setTimeout(target_, didFire_.get());
}

void Timer::extend(Duration timeout) {
  reset();
  target_ += timeout;
  TimerManager::setTimeout(target_, didFire_.get());
}
}
