#pragma once

#include <json/json.hpp>

#include <string>
#include <stdexcept>
#include <fstream>

namespace common {

using json = nlohmann::json;

class Configerator {
public:
  Configerator(std::string configPath);

  static bool hasKey(std::string const& key);

  static std::string getString(std::string const& key);
  static int getInt(std::string const& key);
  static std::vector<std::string> getStringArray(std::string const& key);

private:
  static Configerator* instance_;
  json config_;

  static void assertHasKey(std::string const& key);
};

}
