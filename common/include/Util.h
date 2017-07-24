#pragma once

#include <string>
#include <iostream>

#define LOG() std::cout << logHeader()

std::string logHeader();

void throwUnixError(std::string const& action);
void throwGetAddrInfoError(int err);
void assertTrue(bool condition, std::string const& reason);
bool checkUnixError(int ret, std::string const& action, int allowed = 0);
bool checkRetryableError(int ret, std::string const& action, int allowed = 0);
void logf(char const* format, ...);
