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
  virtual ~StatBase();

protected:
  std::string entity_;
  std::string metric_;

private:
  static size_t seq_;

  size_t id_;
  Prefix prefix_;

  virtual double collect() = 0;

  friend class StatsManager;
};

class StatsManager {
public:
  using SubscribeData = std::map<std::pair<std::string, std::string>, double>;
  using SubscribeCallback = std::function<void(SubscribeData const&)>;

  static void addStat(StatBase* stat);
  static void removeStat(StatBase* stat);

  static void subscribe(SubscribeCallback callback) {
    getInstance().callbacks_.push_back(callback);
  }

  // Collects all the stats the send the aggregated data to all the subscribed
  // callbacks.
  static void collect() {
    auto data = SubscribeData{};

    for (auto stat : getInstance().stats_) {
      data[std::make_pair(stat->entity_, stat->metric_)] = stat->collect();
    }

    for (auto const& callback : getInstance().callbacks_) {
      callback(data);
    }
  }

  template <typename O> static void dump(O& output, SubscribeData const& data) {
    dump(output, data,
         [](std::string const&, std::string const&) { return true; });
  }

  template <typename O>
  static void
  dump(O& output, SubscribeData const& data,
       std::function<bool(std::string const&, std::string const&)> filter) {
    auto byEntity =
        std::map<std::string, std::vector<std::pair<std::string, double>>>{};

    for (auto const& entry : data) {
      byEntity[entry.first.first].push_back(
          std::make_pair(entry.first.second, entry.second));
    }

    for (auto const& entity : byEntity) {
      std::vector<std::pair<std::string, double>> filteredStats;
      for (auto const& stat : entity.second) {
        if (filter(entity.first, stat.first)) {
          filteredStats.push_back(stat);
        }
      }
      if (filteredStats.empty()) {
        continue;
      }

      output << std::left << std::setw(kNamePaddingLength) << entity.first
             << ": ";
      bool isFirst = true;
      for (auto const& stat : filteredStats) {
        if (isFirst) {
          isFirst = false;
        } else {
          output << ", ";
        }
        output << stat.first << " = " << stat.second;
      }
      output << std::endl;
    }
  }

private:
  static const size_t kNamePaddingLength = 10;

  std::set<StatBase*> stats_;
  std::vector<SubscribeCallback> callbacks_;

  static StatsManager& getInstance();
};
} // namespace stats
