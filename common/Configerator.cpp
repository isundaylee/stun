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

/* static */ bool Configerator::hasKey(std::string const& key) {
  return (instance_->config_.find(key) != instance_->config_.end());
}

/* static */ std::string Configerator::getString(std::string const& key) {
  assertHasKey(key);
  return instance_->config_[key];
}

/* static */ int Configerator::getInt(std::string const& key) {
  assertHasKey(key);
  return instance_->config_[key];
}

/* static */ std::vector<std::string>
Configerator::getStringArray(std::string const& key) {
  assertHasKey(key);
  return instance_->config_[key];
}

/* static */ void Configerator::assertHasKey(std::string const& key) {
  assertTrue(hasKey(key), "Cannot find config entry '" + key + "'.");
}
} // namespace common
