#pragma once

#include <json/json.hpp>

#include <string>

namespace common {

using json = nlohmann::json;

class Notebook {
public:
  Notebook(std::string path);

  static Notebook& getInstance();

  void save();
  json& operator[](std::string key);

private:
  Notebook(Notebook const& copy) = delete;
  Notebook& operator=(Notebook const& copy) = delete;

  Notebook(Notebook&& move) = delete;
  Notebook& operator=(Notebook&& move) = delete;

  static Notebook* instance_;

  std::string path_;
  json storage_;
};
} // namespace common
