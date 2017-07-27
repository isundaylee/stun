#include "common/Configerator.h"

namespace common {

Configerator* Configerator::instance_ = nullptr;

Configerator::Configerator(std::string configPath) {
  if (instance_ != nullptr) {
    throw std::runtime_error("Configerator can only be initialized once.");
  }

  std::ifstream input(configPath);
  input >> config_;

  Configerator::instance_ = this;
}

/* static */ std::string Configerator::getString(std::string const& key) {
  if (instance_->config_.find(key) == instance_->config_.end()) {
    throw std::runtime_error("Cannot find config entry '" + key + "'.");
  }

  return instance_->config_[key];
}

}
