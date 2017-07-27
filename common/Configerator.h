#pragma once

#include <json/json.hpp>

#include <string>
#include <stdexcept>
#include <fstream>

namespace common {

using json = nlohmann::json;

class Configerator {
public:
  Configerator(std::string configPath) {
    if (instance_ != nullptr) {
      throw std::runtime_error("Configerator can only be initialized once.");
    }

    std::ifstream input(configPath);
    input >> config_;

    Configerator::instance_ = this;
  }

  static std::string getString(std::string const& key) {
    if (instance_->config_.find(key) == instance_->config_.end()) {
      throw std::runtime_error("Cannot find config entry '" + key + "'.");
    }

    return instance_->config_[key];
  }

private:
  static Configerator* instance_;
  json config_;
};

Configerator* Configerator::instance_ = nullptr;

}
