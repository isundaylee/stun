#pragma once

#include <common/Logger.h>

#include <iostream>
#include <string>

typedef unsigned char Byte;

void throwUnixError(std::string const& action);
void throwGetAddrInfoError(int err);
void assertTrue(bool condition, std::string const& reason);
[[noreturn]] void unreachable(std::string const& reason);
bool checkUnixError(int ret, std::string const& action, int allowed = 0);
bool checkRetryableError(int ret, std::string const& action, int allowed = 0);
