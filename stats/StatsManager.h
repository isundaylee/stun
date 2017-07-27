#pragma once

#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <string>

namespace stats {

class StatsManager;

class StatBase {
protected:
  StatBase(std::string metric);
  ~StatBase();

  bool operator<(StatBase const& rhs) { return id_ < rhs.id_; }

protected:
  std::string name_;
  std::string metric_;

private:
  static size_t seq_;

  size_t id_;

  virtual std::string stringValue() = 0;

  friend class StatsManager;
};

class StatsManager {
public:
  static void addStat(StatBase* stat);
  static void removeStat(StatBase* stat);

  template <typename O> static void dump(O& output) {
    std::map<std::string, std::vector<StatBase*>> statsByName;
    for (StatBase* stat : getInstance().stats_) {
      statsByName[stat->name_].push_back(stat);
    }

    for (auto const& pair : statsByName) {
      output << "Stats for " << pair.first << ": ";
      bool isFirst = true;
      for (auto const& stat : pair.second) {
        if (isFirst) {
          isFirst = false;
        } else {
          output << ", ";
        }
        output << stat->metric_ << " = " << stat->stringValue();
      }
      output << std::endl;
    }
  }

private:
  std::set<StatBase*> stats_;

  static StatsManager& getInstance();
};
}
