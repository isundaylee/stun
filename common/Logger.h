#pragma once

#include <common/Util.h>

#if IOS
#include <os/log.h>
#endif

#include <iostream>
#include <sstream>
#include <stdexcept>

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
      buffer_ << logHeader() << "[" << tag_ << "] ";
      for (size_t i = tag_.length(); i < kLoggerTagPaddingTo; i++) {
        buffer_ << " ";
      }
      linePrimed_ = true;
    }
    buffer_ << v;
    return *this;
  }

  Logger& operator<<(std::ostream& (*os)(std::ostream&)) {
    if (level_ < threshold_) {
      return *this;
    }

    buffer_ << os;
    linePrimed_ = false;
    flush();

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

  std::function<void(std::string)> tee;

private:
  const size_t kTimestampBufferSize = 64;

  bool linePrimed_ = false;
  std::string tag_;
  std::ostream& out_;
  LogLevel threshold_ = VERBOSE;
  LogLevel level_ = VERBOSE;
  std::stringstream buffer_;

  std::string logHeader() {
    char timestampBuffer[kTimestampBufferSize];
    time_t rawTime;
    struct tm* timeInfo;

    time(&rawTime);
    timeInfo = localtime(&rawTime);

    strftime(timestampBuffer, kTimestampBufferSize, "[%F %H:%M:%S] ", timeInfo);
    return std::string(timestampBuffer);
  }

#if IOS
  void flush() {
    static os_log_t logObject = os_log_create("me.ljh.stun", "");

    os_log_error(logObject, "%s", buffer_.str().c_str());

    if (!!tee) {
      tee(buffer_.str());
    }

    buffer_.str("");
  }
#else
  void flush() {
    out_ << buffer_.str();
    out_.flush();

    if (!!tee) {
      tee(buffer_.str());
    }

    buffer_.str("");
  }
#endif
};
}
