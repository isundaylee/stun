#include "common/Util.h"

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>

#include <chrono>
#include <stdexcept>
#include <string>

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
    if (errno != EAGAIN && errno != allowed) {
      throwUnixError(action);
    }
    return false;
  }
  return true;
}
