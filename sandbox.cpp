#include <common/Util.h>

#include <iostream>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  for (auto const& token : split("10.100.0.1", ".")) {
    std::cout << token << std::endl;
  }

  return 0;
}
