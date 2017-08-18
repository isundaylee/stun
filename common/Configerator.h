#pragma once

#include <json/src/json.hpp>

#include <common/Util.h>

#include <fstream>
#include <stdexcept>
#include <string>

namespace common {

using json = nlohmann::json;

class Configerator {
public:
  Configerator(std::string configPath);

  static bool hasKey(std::string const& key);

  static std::string getString(std::string const& key);
  static int getInt(std::string const& key);
  static std::vector<std::string> getStringArray(std::string const& key);

  template <typename T> static T get(std::string key) {
    assertTrue(hasKey(key), "Cannot find config entry '" + key + "'.");
    return instance_->config_[key].get<T>();
  }

  template <typename T> static T get(std::string key, T defaultValue) {
    if (!hasKey(key)) {
      return defaultValue;
    }

    T result = instance_->config_[key];
    return result;
  }

  json static getJSON() { return instance_->config_; }

private:
  static Configerator* instance_;
  json config_;

  static void assertHasKey(std::string const& key);
};
}
