#pragma once

#include <common/Logger.h>

#include <iostream>
#include <string>
#include <vector>

#if defined(__linux__)
#define LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
#define OSX 1
#endif

typedef unsigned char Byte;

void throwUnixError(std::string const& action);
void throwGetAddrInfoError(int err);
void assertTrue(bool condition, std::string const& reason);
[[noreturn]] void unreachable(std::string const& reason);
bool checkUnixError(int ret, std::string const& action, int allowed = 0);
bool checkRetryableError(int ret, std::string const& action, int allowed = 0);
std::string runCommand(std::string command);
std::vector<std::string> split(std::string const& string,
                               std::string const& separator);
