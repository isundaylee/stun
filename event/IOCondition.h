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
  IOCondition(int fd, IOType type)
      : BaseCondition(ConditionType::IO), fd(fd), type(type) {}

  int fd;
  IOType type;
};

class IOConditionManager : ConditionManager {
public:
  static IOCondition* canRead(int fd);
  static IOCondition* canWrite(int fd);
  static void close(int fd);

  virtual void
  prepareConditions(std::vector<Condition*> const& conditions,
                    std::vector<Condition*> const& interesting) override;

private:
  IOConditionManager();

  IOConditionManager(IOConditionManager const& copy) = delete;
  IOConditionManager& operator=(IOConditionManager const& copy) = delete;

  IOConditionManager(IOConditionManager const&& move) = delete;
  IOConditionManager& operator=(IOConditionManager const&& move) = delete;

  std::map<std::pair<IOType, int>, std::unique_ptr<IOCondition>> conditions_;

  static IOConditionManager& getInstance();

  IOCondition* canDo(int fd, IOType type);
  void removeCondition(int fd, IOType type);
};
}
