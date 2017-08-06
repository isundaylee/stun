#pragma once

#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <string>

namespace stats {

enum Prefix {
  None = 1,
  Kilo = 1 << 10,
  Mega = 1 << 20,
  Giga = 1 << 30,
  Tera = 1 << 40,
};

class StatsManager;

class StatBase {
protected:
  StatBase(std::string entity, std::string metric,
           Prefix prefix = Prefix::None);
  ~StatBase();

protected:
  std::string entity_;
  std::string metric_;

private:
  static size_t seq_;

  size_t id_;
  Prefix prefix_;

  virtual std::string collect() = 0;

  friend class StatsManager;
};

class StatsManager {
public:
  static void addStat(StatBase* stat);
  static void removeStat(StatBase* stat);

  template <typename O> static void dump(O& output) {
    dump(output, [](std::string const&, std::string const&) { return true; });
  }

  template <typename O>
  static void
  dump(O& output,
       std::function<bool(std::string const&, std::string const&)> filter) {
    std::map<std::string, std::vector<StatBase*>> statsByName;
    for (StatBase* stat : getInstance().stats_) {
      statsByName[stat->entity_].push_back(stat);
    }

    for (auto const& pair : statsByName) {
      std::vector<StatBase*> filteredStats;
      for (auto const& stat : pair.second) {
        if (filter(stat->entity_, stat->metric_)) {
          filteredStats.push_back(stat);
        }
      }
      if (filteredStats.empty()) {
        continue;
      }

      output << std::left << std::setw(kNamePaddingLength) << pair.first
             << ": ";
      bool isFirst = true;
      for (auto const& stat : filteredStats) {
        if (isFirst) {
          isFirst = false;
        } else {
          output << ", ";
        }
        output << stat->metric_ << " = " << stat->collect();
      }
      output << std::endl;
    }
  }

private:
  static const size_t kNamePaddingLength = 10;

  std::set<StatBase*> stats_;

  static StatsManager& getInstance();
};
}
