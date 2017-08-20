#include "event/IOCondition.h"

#include <event/EventLoop.h>

#include <errno.h>
#include <poll.h>
#include <string.h>

#include <iostream>
#include <stdexcept>

namespace event {

static const int kIOPollTimeout = 1;

#if LINUX
const int kReadPollMask = POLLIN | POLLPRI | POLLRDHUP | POLLHUP;
#else
const int kReadPollMask = POLLIN | POLLPRI | POLLHUP;
#endif

const int kWritePollMask = POLLOUT;

IOConditionManager::IOConditionManager() : conditions_() {
  EventLoop::getCurrentLoop().addConditionManager(this, ConditionType::IO);
}

/* static */ IOConditionManager& IOConditionManager::getInstance() {
  static IOConditionManager instance;
  return instance;
}

/* static */ IOCondition* IOConditionManager::canRead(int fd) {
  return getInstance().canDo(fd, IOType::Read);
}

/* static */ IOCondition* IOConditionManager::canWrite(int fd) {
  return getInstance().canDo(fd, IOType::Write);
}

/* static */ void IOConditionManager::close(int fd) {
  getInstance().removeCondition(fd, IOType::Read);
  getInstance().removeCondition(fd, IOType::Write);
}

void IOConditionManager::prepareConditions(
    std::vector<Condition*> const& conditions,
    std::vector<Condition*> const& interesting) {
  // First reset all conditions
  for (auto condition : conditions) {
    static_cast<IOCondition*>(condition)->arm();
  }

  // Let's poll!
  struct pollfd polls[interesting.size()];
  for (size_t i = 0; i < interesting.size(); i++) {
    IOCondition* condition = static_cast<IOCondition*>(interesting[i]);
    polls[i].fd = condition->fd;
    polls[i].events =
        (condition->type == IOType::Read ? kReadPollMask : kWritePollMask);
    polls[i].revents = 0;
  }
  int ret = poll(polls, interesting.size(), kIOPollTimeout);

  if (ret < 0) {
    if (errno == EINTR) {
      // We have probably been interrupted by signals set by another condition
      // type. We should return and give the runloop a chance to proceed.
      return;
    }

    throw std::runtime_error("Error encountered while poll()-ing: " +
                             std::string(strerror(errno)));
  }

  // Enable connections according to poll result
  for (size_t i = 0; i < interesting.size(); i++) {
    if (polls[i].revents & POLLNVAL) {
      throw std::runtime_error("Invalid file descriptor. Be sure to call "
                               "IOConditionManager::close(fd) before "
                               "close()-ing the file descriptor.");
    }

    IOCondition* condition = static_cast<IOCondition*>(interesting[i]);
    int mask =
        (condition->type == IOType::Read ? kReadPollMask : kWritePollMask);
    if (polls[i].revents & mask) {
      condition->fire();
    }
  }
}

IOCondition* IOConditionManager::canDo(int fd, IOType type) {
  auto existing = conditions_.find(std::make_pair(type, fd));
  if (existing != conditions_.end()) {
    return existing->second.get();
  }

  IOCondition* condition = new IOCondition(fd, type);
  conditions_[std::make_pair(type, fd)].reset(condition);
  return condition;
}

void IOConditionManager::removeCondition(int fd, IOType type) {
  auto existing = conditions_.find(std::make_pair(type, fd));
  if (existing != conditions_.end()) {
    conditions_.erase(existing);
  }
}
}
