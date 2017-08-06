#include "stats/StatsManager.h"

namespace stats {

/* static */ size_t StatBase::seq_ = 0;

StatBase::StatBase(std::string entity, std::string metric,
                   Prefix prefix /* = Prefix::None */)
    : entity_(entity), metric_(metric) {
  id_ = StatBase::seq_;
  StatBase::seq_++;
  prefix_ = prefix;
  StatsManager::addStat(this);
}

StatBase::~StatBase() { StatsManager::removeStat(this); }

/* static */ StatsManager& StatsManager::getInstance() {
  static StatsManager instance;
  return instance;
}

/* static */ void StatsManager::addStat(StatBase* stat) {
  getInstance().stats_.insert(stat);
}

/* static */ void StatsManager::removeStat(StatBase* stat) {
  getInstance().stats_.erase(stat);
}
}
