#include "common/Util.h"

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>

#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string>

static const size_t kUtilRunCommandOutputBufferSize = 1024;

void throwUnixError(std::string const& action) {
  throw std::runtime_error("Error while " + action + ": " +
                           std::string(strerror(errno)));
}

void throwGetAddrInfoError(int err) {
  throw std::runtime_error("Error while getaddrinfo(): " +
                           std::string(gai_strerror(err)));
}

void assertTrue(bool condition, std::string const& reason) {
  if (!condition) {
#if TARGET_BSD
    // C++ on BSD doesn't seem to print exception upon abort.
    LOG_E("Util") << reason << std::endl;
#endif

    throw std::runtime_error(reason);
  }
}

void unreachable(std::string const& reason) {
  throw std::runtime_error(reason);
}

void notImplemented(std::string const& reason) {
  LOG_E("Util") << "Not implemented: " << reason << std::endl;
  throw std::runtime_error("Not implemented: " + reason);
}

bool checkUnixError(int ret, std::string const& action, int allowed /* = 0 */) {
  if (ret < 0) {
    if (errno != allowed) {
      throwUnixError(action);
    }
    return false;
  }
  return true;
}

bool checkRetryableError(int ret, std::string const& action,
                         int allowed /* = 0 */) {
  if (ret < 0) {
    if (errno != EAGAIN && errno != EINTR && errno != allowed) {
      throwUnixError(action);
    }
    return false;
  }
  return true;
}

RunCommandResult runCommand(std::string command) {
  char buffer[kUtilRunCommandOutputBufferSize];
  std::stringstream result;
  FILE* pipe = popen((command + " 2>/dev/null").c_str(), "r");

  assertTrue(pipe != NULL, "Cannot popen() while executing: " + command);

  while (!feof(pipe)) {
    if (fgets(buffer, kUtilRunCommandOutputBufferSize, pipe) != nullptr) {
      result << buffer;
    }
  }

  int ret = pclose(pipe);

  return RunCommandResult{ret, result.str()};
}

RunCommandResult runCommandAndAssertSuccess(std::string command) {
  auto result = runCommand(command);

  assertTrue(result.exitCode == 0, "Error code " +
                                       std::to_string(result.exitCode) +
                                       " while executing: " + command);

  return result;
}

std::vector<std::string> split(std::string const& string,
                               std::string const& separator) {
  auto tokens = std::vector<std::string>{};
  auto found = string.find(separator), start = 0UL;

  while (found != std::string::npos) {
    tokens.push_back(string.substr(start, found - start));
    start = found + separator.length();
    found = string.find(separator, found + separator.length());
  }

  tokens.push_back(string.substr(start, string.length() - start));

  return tokens;
}
