#include "event/Timer.h"

#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

namespace event {

class TimerManager {
public:
  static void setTimeout(Time target, Condition* condition);
  static void removeTimeout(Condition* condition);

private:
  TimerManager();

  typedef std::pair<Time, Condition*> TimeoutTrigger;

  static TimerManager* instance_;
  std::priority_queue<TimeoutTrigger> targets_;
  std::set<Condition*> removed_;
  Time currentTarget_ = 0;
  timer_t timer_;

  static void handleSignal(int sig, siginfo_t* si, void* uc);

  void updateTimer(Time now);
  void fireUntilTarget();
};

const int kTimerManagerSignal = SIGRTMIN;
const Duration kMillisecondsInASecond = 1000;
const Duration kNanosecondsInAMillisecond = 1000000;

TimerManager* TimerManager::instance_ = new TimerManager();

TimerManager::TimerManager() {
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &TimerManager::handleSignal;
  sigemptyset(&sa.sa_mask);

  // Bind the timer singal handler
  if (sigaction(kTimerManagerSignal, &sa, NULL) < 0) {
    throw std::runtime_error("Cannot bind timer signal handler.");
  }

  // Create the timer
  struct sigevent sev;
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = kTimerManagerSignal;
  sev.sigev_value.sival_ptr = &timer_;
  if (timer_create(CLOCK_MONOTONIC, &sev, &timer_) < 0) {
    throw std::runtime_error("Cannot create timer.");
  }
}

/* static */ void TimerManager::setTimeout(Time target, Condition* condition) {
  Time now = Timer::getTimeInMilliseconds();
  instance_->targets_.emplace(target, condition);
  instance_->updateTimer(now);
}

/* static */ void TimerManager::removeTimeout(Condition* condition) {
  instance_->removed_.insert(condition);
}

Time Timer::Timer::getTimeInMilliseconds() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * kMillisecondsInASecond +
          ts.tv_nsec / kNanosecondsInAMillisecond);
}

void TimerManager::handleSignal(int sig, siginfo_t* si, void* uc) {
  instance_->fireUntilTarget();
}

void TimerManager::fireUntilTarget() {
  Time now = Timer::getTimeInMilliseconds();
  Time target = std::max(now, currentTarget_);
  while (!targets_.empty() && targets_.top().first <= target) {
    TimeoutTrigger fired = targets_.top();
    targets_.pop();

    if (removed_.find(fired.second) != removed_.end()) {
      // This timer has already been removed. We should not fire it.
      removed_.erase(fired.second);
      continue;
    }

    fired.second->value = true;
  }
  if (targets_.empty()) {
    return;
  }
  updateTimer(now);
}

void TimerManager::updateTimer(Time now) {
  if (currentTarget_ != 0 && targets_.top().first >= currentTarget_) {
    return;
  }

  Duration timeout = std::max(1L, (int64_t)targets_.top().first - (int64_t)now);

  struct itimerspec its;
  its.it_value.tv_sec = timeout / kMillisecondsInASecond;
  its.it_value.tv_nsec =
      (timeout % kMillisecondsInASecond) * kNanosecondsInAMillisecond;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;

  if (timer_settime(instance_->timer_, 0, &its, NULL) < 0) {
    throw std::runtime_error("Cannot set timer time.");
  }
}

Timer::Timer(Duration timeout) : didFire_(new Condition()) { reset(timeout); }

Timer::~Timer() { TimerManager::removeTimeout(didFire_.get()); }

Condition* Timer::didFire() { return didFire_.get(); }

void Timer::reset() { didFire_.get()->value = false; }

void Timer::reset(Duration timeout) {
  reset();
  Time now = getTimeInMilliseconds();
  target_ = now + timeout;
  TimerManager::setTimeout(target_, didFire_.get());
}

void Timer::extend(Duration timeout) {
  reset();
  target_ += timeout;
  TimerManager::setTimeout(target_, didFire_.get());
}
}
