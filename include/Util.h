#pragma once

#include <string>
#include <iostream>

#define LOG() std::cout << stun::logHeader()

namespace stun {

std::string logHeader();

void throwUnixError(std::string const& action);

void throwGetAddrInfoError(int err);

void logf(char const* format, ...);

}
