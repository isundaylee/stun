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
    throw std::runtime_error(reason);
  }
}

void unreachable(std::string const& reason) {
  throw std::runtime_error(reason);
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

std::string runCommand(std::string command) {
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
  assertTrue(ret == 0,
             "Error code " + std::to_string(ret) +
                 " while executing: " + command);

  return result.str();
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
