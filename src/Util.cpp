#include "Util.h"

#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <string>
#include <stdexcept>

namespace stun {

void throwUnixError(std::string const& action) {
  throw std::runtime_error("Error while " + action + ": " + std::string(strerror(errno)));
}

void logf(char const* format, ...) {
  va_list argptr;
  va_start(argptr, format);
  vprintf(format, argptr);
  va_end(argptr);
}

}
