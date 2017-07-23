#include "Util.h"

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <netdb.h>

#include <string>
#include <stdexcept>
#include <chrono>

namespace stun {

const size_t kUtilTimestampBufferSize = 64;

std::string logHeader() {
  char timestampBuffer[kUtilTimestampBufferSize];
  time_t rawTime;
  struct tm* timeInfo;

  time(&rawTime);
  timeInfo = localtime(&rawTime);

  strftime(timestampBuffer, kUtilTimestampBufferSize, "[%F %H:%M:%S] ", timeInfo);
  return std::string(timestampBuffer);
}

void throwUnixError(std::string const& action) {
  throw std::runtime_error("Error while " + action + ": " + std::string(strerror(errno)));
}

void throwGetAddrInfoError(int err) {
  throw std::runtime_error("Error while getaddrinfo(): " + std::string(gai_strerror(err)));
}

void assertTrue(bool condition, std::string const& reason) {
  if (!condition) {
    throw std::runtime_error(reason);
  }
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

bool checkRetryableError(int ret, std::string const& action, int allowed /* = 0 */) {
  if (ret < 0) {
    if (errno != EAGAIN && errno != allowed) {
      throwUnixError(action);
    }
    return false;
  }
  return true;
}

void logf(char const* format, ...) {
  va_list argptr;
  va_start(argptr, format);
  vprintf(format, argptr);
  va_end(argptr);
}

}
