#pragma once

#include <event/Condition.h>
#include <event/EventLoop.h>

#include <map>
#include <memory>

namespace event {

enum IOType {
  Read,
  Write,
};

class IOCondition : public BaseCondition {
public:
  IOCondition(event::EventLoop& loop, int fd, IOType type)
      : BaseCondition(loop, ConditionType::IO), fd(fd), type(type) {}

  int fd;
  IOType type;
};

class IOConditionManager : ConditionManager {
public:
  IOConditionManager(EventLoop& loop);

  IOCondition* canRead(int fd);
  IOCondition* canWrite(int fd);
  void close(int fd);

  virtual void
  prepareConditions(std::vector<Condition*> const& conditions) override;

private:
  IOConditionManager(IOConditionManager const& copy) = delete;
  IOConditionManager& operator=(IOConditionManager const& copy) = delete;

  IOConditionManager(IOConditionManager const&& move) = delete;
  IOConditionManager& operator=(IOConditionManager const&& move) = delete;

  EventLoop& loop_;

  std::map<std::pair<IOType, int>, std::unique_ptr<IOCondition>> conditions_;

  IOCondition* canDo(int fd, IOType type);
  void removeCondition(int fd, IOType type);
};
} // namespace event
