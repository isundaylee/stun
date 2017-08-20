#include "common/Notebook.h"

#include <unistd.h>

#include <common/Util.h>

#include <fstream>

namespace common {

/* static */ Notebook* Notebook::instance_ = nullptr;

Notebook::Notebook(std::string path) : path_(path) {
  if (access(path.c_str(), F_OK) != -1) {
    // Notebook file exists already
    std::ifstream input(path);
    input >> storage_;
  } else {
    storage_ = json({});
  }

  Notebook::instance_ = this;
}

/* static */ Notebook& Notebook::getInstance() {
  assertTrue(Notebook::instance_ != nullptr, "No Notebook present.");

  return *Notebook::instance_;
}

void Notebook::save() {
  std::ofstream output(path_);
  output << storage_.dump() << std::endl;
}

json& Notebook::operator[](std::string key) { return storage_[key]; }
}
