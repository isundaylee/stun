#include "event/IOCondition.h"

#include <event/EventLoop.h>

#include <poll.h>
#include <string.h>
#include <errno.h>

#include <iostream>
#include <stdexcept>

namespace event {

const int kReadPollMask = POLLIN | POLLPRI | POLLRDHUP | POLLHUP;
const int kWritePollMask = POLLOUT;

IOConditionManager* IOConditionManager::instance = nullptr;

IOConditionManager::IOConditionManager() :
    conditions_() {
  EventLoop::getCurrentLoop()->addConditionManager(this, ConditionType::IO);
}

/* static */ IOConditionManager* IOConditionManager::getInstance() {
  if (IOConditionManager::instance == nullptr) {
    IOConditionManager::instance = new IOConditionManager();
  }

  return IOConditionManager::instance;
}

/* static */ IOCondition* IOConditionManager::canRead(int fd) {
  return getInstance()->canDo(fd, IOType::Read);
}

/* static */ IOCondition* IOConditionManager::canWrite(int fd) {
  return getInstance()->canDo(fd, IOType::Write);
}

void IOConditionManager::prepareConditions(std::vector<Condition*> const& conditions) {
  // Resetting all conditions first
  for (auto condition : conditions) {
    condition->value = false;
  }

  // Let's poll!
  struct pollfd polls[conditions.size()];
  for (size_t i=0; i<conditions.size(); i++) {
    IOCondition* condition = (IOCondition*) conditions[i];
    polls[i].fd = condition->fd;
    polls[i].events = (condition->type == IOType::Read ? kReadPollMask : kWritePollMask);
    polls[i].revents = 0;
  }
  int ret = poll(polls, conditions.size(), -1);

  if (ret < 0) {
    throw std::runtime_error("Error encountered while poll()-ing: " + std::string(strerror(errno)));
  }

  // Enable conditions according to poll result
  for (size_t i=0; i<conditions.size(); i++) {
    IOCondition* condition = (IOCondition*) conditions[i];
    int mask = (condition->type == IOType::Read ? kReadPollMask : kWritePollMask);
    if (polls[i].revents & (POLLERR | POLLNVAL)) {
      throw std::runtime_error("Error response from poll()");
    }

    if (polls[i].revents & mask) {
      condition->value = true;
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

}
