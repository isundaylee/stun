#pragma once

#include <iostream>

#define LOG() ::common::Logger::getDefault()

namespace common {

class Logger {
public:
  Logger(std::ostream& out = std::cout) : out_(out) {}

  template <typename T> Logger& operator<<(const T& v) {
    if (!linePrimed_) {
      out_ << logHeader();
      linePrimed_ = true;
    }
    out_ << v;
    return *this;
  }

  Logger& operator<<(std::ostream& (*os)(std::ostream&)) {
    out_ << os;
    linePrimed_ = false;
    return *this;
  }

  static Logger& getDefault() {
    static Logger defaultLogger;
    return defaultLogger;
  }

private:
  const size_t kTimestampBufferSize = 64;

  bool linePrimed_ = false;
  std::ostream& out_;

  std::string logHeader() {
    char timestampBuffer[kTimestampBufferSize];
    time_t rawTime;
    struct tm* timeInfo;

    time(&rawTime);
    timeInfo = localtime(&rawTime);

    strftime(timestampBuffer, kTimestampBufferSize, "[%F %H:%M:%S] ", timeInfo);
    return std::string(timestampBuffer);
  }
};
}
