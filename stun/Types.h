#pragma once

#include <json/json.hpp>

namespace stun {

enum class DataPipeType {
  UDP,
  TCP,
};

inline void to_json(nlohmann::json& j, DataPipeType const& type) {
  switch (type) {
  case DataPipeType::UDP:
    j = "udp";
    break;
  case DataPipeType::TCP:
    j = "tcp";
    break;
  }
}

inline void from_json(nlohmann::json const& j, DataPipeType& type) {
  if (j == "udp") {
    type = DataPipeType::UDP;
  } else if (j == "tcp") {
    type = DataPipeType::TCP;
  } else {
    throw std::invalid_argument("Unknown DataPipeType: " + j.dump());
  }
}

} // namespace stun