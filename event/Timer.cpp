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

TimerManager::Core::Core()
    : statMinRemainingTimeUSec_{"Timer", "min_remaining_time_usec"},
      statTimesWaited_{"Timer", "times_waited"}, statTimesStopped_{
                                                     "Timer", "times_stopped"} {
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = &TimerManager::Core::handleSignal;
  sigemptyset(&sa.sa_mask);

  // Bind the timer singal handler
  if (sigaction(SIGALRM, &sa, NULL) < 0) {
    throw std::runtime_error("Cannot bind timer signal handler.");
  }
}

void TimerManager::Core::addManager(TimerManager* manager) {
  managers_.push_back(manager);
}

void TimerManager::Core::removeManager(TimerManager* manager) {
  auto found = std::find(managers_.begin(), managers_.end(), manager);

  assertTrue(
      found != managers_.end(),
      "Attempting to remove an unknown manager from TimerManager::Core.");

  managers_.erase(found);
}

/* static */ TimerManager::Core& TimerManager::Core::getInstance() {
  static TimerManager::Core instance;
  return instance;
}

static sigset_t getTimerSignalSet() {
  sigset_t set;
  sigemptyset(&set);

  int ret = sigaddset(&set, SIGALRM);
  checkUnixError(ret, "calling sigaddset()");

  return set;
}

void TimerManager::Core::requestTimeout(Time target) {
  maskSignal();

  // If the requested target is not in the future.
  if (target <= masterClock_) {
    unmaskSignal();
    return;
  }

  // If we have a pending target that is before the requested target.
  if (pending_ && currentTarget_ <= target) {
    unmaskSignal();
    return;
  }

  // First, we stop the current timer (if any).
  if (pending_) {
    struct itimerval old;
    int ret = getitimer(ITIMER_REAL, &old);
    checkUnixError(ret, "getitimer()");

    static auto const kWaitThreshold = std::chrono::microseconds{100};
    auto remainingTime = std::chrono::seconds(old.it_value.tv_sec) +
                         std::chrono::microseconds(old.it_value.tv_usec);
    if (remainingTime <= kWaitThreshold) {
      // The current pending timer is nearing its expiration / has already
      // expired. In this case, to be safe, we just wait out its expiration, and
      // consume the pending signal. If we do not consume the pending signal,
      // after we unblock the signal, the signal handler might be triggered with
      // the wrong currentTarget set.
      sigset_t sigset = getTimerSignalSet();
      int sigConsumed;
      int ret = sigwait(&sigset, &sigConsumed);
      checkUnixError(ret, "sigwait()");

      statTimesWaited_.accumulate();
    } else {
      // The expiration is still far away enough. We can safely stop the timer.
      struct itimerval stop = {{0, 0}, {0, 0}};
      int ret = setitimer(ITIMER_REAL, &stop, NULL);
      checkUnixError(ret, "setitimer() while stopping");

      statTimesStopped_.accumulate();
    }

    statMinRemainingTimeUSec_.accumulate(
        std::chrono::duration_cast<std::chrono::microseconds>(remainingTime)
            .count());
  }

  // Then, we calculate and set the new timer.
  Duration timeout =
      std::max(std::chrono::milliseconds(1),
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   target - Timer::getTime()));

  struct itimerval its;
  its.it_value.tv_sec = timeout / 1s;
  its.it_value.tv_usec =
      std::chrono::duration_cast<std::chrono::microseconds>(timeout % 1s)
          .count();
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_usec = 0;

  currentTarget_ = target;
  pending_ = true;

  int ret = setitimer(ITIMER_REAL, &its, NULL);
  checkUnixError(ret, "setitimer() while (re)starting");

  unmaskSignal();
}

void TimerManager::Core::maskSignal() {
  auto set = getTimerSignalSet();
  int ret = sigprocmask(SIG_BLOCK, &set, NULL);
  checkUnixError(ret, "calling sigprocmask()");
}

void TimerManager::Core::unmaskSignal() {
  auto set = getTimerSignalSet();
  int ret = sigprocmask(SIG_UNBLOCK, &set, NULL);
  checkUnixError(ret, "calling sigprocmask()");
}

// See handleTimeoutAssertMessage comment.
static std::string const handleSignalAssertMessage =
    "How can time not go forward in handleSignal?";
void TimerManager::Core::handleSignal(int sig, siginfo_t* si, void* uc) {
  // TODO: Potential race condition?

  assertTrue(TimerManager::Core::getInstance().currentTarget_ >
                 TimerManager::Core::getInstance().masterClock_,
             handleSignalAssertMessage);

  TimerManager::Core::getInstance().masterClock_ =
      TimerManager::Core::getInstance().currentTarget_;
  TimerManager::Core::getInstance().pending_ = false;

  for (auto manager : TimerManager::Core::getInstance().managers_) {
    manager->handleTimeout(TimerManager::Core::getInstance().masterClock_);
  }
}

TimerManager::TimerManager(EventLoop& loop) : loop_(loop) {
  loop_.addConditionManager(this, ConditionType::TimerSignal);
  TimerManager::Core::getInstance().addManager(this);
}

/* virtual */ TimerManager::~TimerManager() {
  TimerManager::Core::getInstance().removeManager(this);
}

/* virtual */ void
TimerManager::prepareConditions(std::vector<Condition*> const& conditions) {
  TimerManager::Core::getInstance().maskSignal();

  while (!targets_.empty() && targets_.back().first <= clock_) {
    TimeoutTrigger fired = targets_.back();
    targets_.pop_back();
    fired.second->fire();
  }

  TimerManager::Core::getInstance().unmaskSignal();

  updateTimer();
}

void TimerManager::setTimeout(Time target, BaseCondition* condition) {
  auto existing = std::find_if(targets_.begin(), targets_.end(),
                               [condition](TimeoutTrigger const& trigger) {
                                 return trigger.second == condition;
                               });

  if (existing == targets_.end()) {
    targets_.emplace_back(target, condition);
  } else {
    existing->first = target;
  }

  std::sort(targets_.begin(), targets_.end(), std::greater<TimeoutTrigger>());

  updateTimer();
}

void TimerManager::removeTimeout(BaseCondition* condition) {
  auto existing = std::find_if(targets_.begin(), targets_.end(),
                               [condition](TimeoutTrigger const& trigger) {
                                 return trigger.second == condition;
                               });

  if (existing != targets_.end()) {
    targets_.erase(existing);
  }
}

Time Timer::getTime() { return std::chrono::steady_clock::now(); }

Duration Timer::getEpochTimeInMilliseconds() {
  return std::chrono::duration_cast<Duration>(getTime().time_since_epoch());
}

void TimerManager::updateTimer() {
  if (targets_.empty()) {
    return;
  }

  TimerManager::Core::getInstance().requestTimeout(targets_.back().first);
}

// assertMessage must be declared here. If it were to be declared within
// handleTimeout, its construction might trigger a heap allocation, which is
// prohibited within a signal handler. Since TimerManager::handleTimeout is
// called in TimerManager::Core::handleSignal, heap allocations must be avoided.
static std::string const handleTimeoutAssertMessage =
    "How can time go backwards?";
void TimerManager::handleTimeout(Time target) {
  assertTrue(target >= clock_, handleTimeoutAssertMessage);
  clock_ = target;
}

Timer::Timer(TimerManager& manager)
    : manager_(manager), didFire_(manager_.loop_.createBaseCondition()) {}

Timer::Timer(TimerManager& manager, Duration timeout)
    : manager_(manager),
      didFire_(manager_.loop_.createBaseCondition(ConditionType::TimerSignal)) {
  reset(timeout);
}

Timer::~Timer() { manager_.removeTimeout(didFire_.get()); }

Condition* Timer::didFire() { return didFire_.get(); }

void Timer::reset() { didFire_.get()->arm(); }

void Timer::reset(Duration timeout) {
  reset();
  Time now = getTime();
  target_ = now + timeout;
  manager_.setTimeout(target_, didFire_.get());
}

void Timer::extend(Duration timeout) {
  reset();
  target_ += timeout;
  manager_.setTimeout(target_, didFire_.get());
}
} // namespace event
