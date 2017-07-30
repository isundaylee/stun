#pragma once

#include <iostream>

#define LOG() ::common::Logger::getDefault("Untagged")
#define LOG_T(tag) ::common::Logger::getDefault(tag)

namespace common {

static const size_t kLoggerTagPaddingTo = 9;

class Logger {
public:
  Logger(std::ostream& out = std::cout) : out_(out) {}

  template <typename T> Logger& operator<<(const T& v) {
    if (!linePrimed_) {
      out_ << logHeader() << "[" << tag_ << "] ";
      for (size_t i = tag_.length(); i < kLoggerTagPaddingTo; i++) {
        out_ << " ";
      }
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

  static Logger& getDefault(std::string const& tag) {
    static Logger defaultLogger;
    defaultLogger.tag_ = tag;
    return defaultLogger;
  }

private:
  const size_t kTimestampBufferSize = 64;

  bool linePrimed_ = false;
  std::string tag_;
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
