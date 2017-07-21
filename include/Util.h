#pragma once

#include <string>
#include <iostream>

#define LOG() std::cout

namespace stun {

void throwUnixError(std::string const& action);

void logf(char const* format, ...);

}
