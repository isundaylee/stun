#pragma once

#include <common/Logger.h>

#include <iostream>
#include <string>
#include <vector>

// clang-format off
#if defined(__linux__)
  #define TARGET_LINUX 1
#elif defined(__APPLE__)
  #include "TargetConditionals.h"
  #if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
    #define TARGET_IOS 1
    #define IOS_SIMULATOR 1
  #elif TARGET_OS_IPHONE
    #define TARGET_IOS 1
  #else
    #define TARGET_OSX 1
  #endif
#elif defined(__unix__)
  #include <sys/param.h>
  #if defined(BSD)
    #define TARGET_BSD 1
  #else
    #error "Unsupported UNIX platform."
  #endif
#else
  #error "Unsupported platform."
#endif
// clang-format on

typedef unsigned char Byte;

struct RunCommandResult {
  int exitCode;
  std::string stdout;
};

void throwUnixError(std::string const& action);
void throwGetAddrInfoError(int err);
void assertTrue(bool condition, std::string const& reason);
[[noreturn]] void unreachable(std::string const& reason);
[[noreturn]] void notImplemented(std::string const& reason);
bool checkUnixError(int ret, std::string const& action, int allowed = 0);
bool checkRetryableError(int ret, std::string const& action, int allowed = 0);
RunCommandResult runCommand(std::string command);
RunCommandResult runCommandAndAssertSuccess(std::string command);
std::vector<std::string> split(std::string const& string,
                               std::string const& separator);
