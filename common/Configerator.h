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

  static std::string getString(std::string const& key);

private:
  static Configerator* instance_;
  json config_;
};

}
