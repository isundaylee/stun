#pragma once

#include <iostream>

#define L()                                                                    \
  ::common::Logger::getDefault("=======").withLogLevel(common::LogLevel::INFO)
#define LOG_E(tag)                                                             \
  ::common::Logger::getDefault(tag).withLogLevel(common::LogLevel::ERROR)
#define LOG_I(tag)                                                             \
  ::common::Logger::getDefault(tag).withLogLevel(common::LogLevel::INFO)
#define LOG_V(tag)                                                             \
  ::common::Logger::getDefault(tag).withLogLevel(common::LogLevel::VERBOSE)

namespace common {

static const size_t kLoggerTagPaddingTo = 9;

enum LogLevel { VERBOSE, INFO, ERROR };

class Logger {
public:
  Logger(std::ostream& out = std::cout) : out_(out) {}

  template <typename T> Logger& operator<<(const T& v) {
    if (level_ < threshold_) {
      return *this;
    }

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
    if (level_ < threshold_) {
      return *this;
    }

    out_ << os;
    linePrimed_ = false;
    return *this;
  }

  static Logger& getDefault(std::string const& tag) {
    static Logger defaultLogger;
    defaultLogger.tag_ = tag;
    return defaultLogger;
  }

  Logger& withLogLevel(LogLevel level) {
    level_ = level;
    return *this;
  }

  void setLoggingThreshold(LogLevel threshold) { threshold_ = threshold; }

private:
  const size_t kTimestampBufferSize = 64;

  bool linePrimed_ = false;
  std::string tag_;
  std::ostream& out_;
  LogLevel threshold_ = VERBOSE;
  LogLevel level_ = VERBOSE;

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
