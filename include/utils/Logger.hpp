#ifndef NODO_UTILS_LOGGER_HPP
#define NODO_UTILS_LOGGER_HPP

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace nodo::utils {

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR };

inline const char *logLevelName(LogLevel level) {
  switch (level) {
  case LogLevel::TRACE: return "TRACE";
  case LogLevel::DEBUG: return "DEBUG";
  case LogLevel::INFO: return "INFO";
  case LogLevel::WARN: return "WARN";
  case LogLevel::ERROR: return "ERROR";
  }
  return "ERROR";
}

inline LogLevel configuredLogLevel() {
  static const LogLevel level = [] {
    const char *value = std::getenv("NODO_LOG_LEVEL");
    if (!value) return LogLevel::WARN;
    const std::string_view name(value);
    if (name == "TRACE") return LogLevel::TRACE;
    if (name == "DEBUG") return LogLevel::DEBUG;
    if (name == "INFO") return LogLevel::INFO;
    if (name == "ERROR") return LogLevel::ERROR;
    return LogLevel::WARN;
  }();
  return level;
}

inline std::string escapeLogField(std::string_view value) {
  constexpr char digits[] = "0123456789abcdef";
  std::string escaped;
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') {
      escaped += '\\';
      escaped += static_cast<char>(c);
    } else if (c < 0x20 || c >= 0x80) {
      escaped += "\\u00";
      escaped += digits[c >> 4];
      escaped += digits[c & 0xf];
    } else {
      escaped += static_cast<char>(c);
    }
  }
  return escaped;
}

class LogLine {
public:
  LogLine(LogLevel level, std::string_view component)
      : m_level(level), m_component(component),
        m_enabled(level >= configuredLogLevel()) {}
  LogLine(const LogLine &) = delete;
  LogLine &operator=(const LogLine &) = delete;

  ~LogLine() noexcept {
    if (!m_enabled) return;
    try {
      static std::mutex sinkMutex;
      const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      std::lock_guard<std::mutex> lock(sinkMutex);
      std::clog << "{\"time_unix_ms\":" << timestamp
                << ",\"level\":\"" << logLevelName(m_level)
                << "\",\"component\":\"" << escapeLogField(m_component)
                << "\",\"message\":\"" << escapeLogField(m_stream.str())
                << "\"}\n";
    } catch (...) {
      // Logging must never change protocol control flow.
    }
  }

  template <typename T> LogLine &operator<<(const T &value) noexcept {
    if (m_enabled) {
      try { m_stream << value; } catch (...) { m_enabled = false; }
    }
    return *this;
  }
  LogLine &operator<<(std::ostream &(*manipulator)(std::ostream &)) noexcept {
    if (m_enabled) {
      try { manipulator(m_stream); } catch (...) { m_enabled = false; }
    }
    return *this;
  }

private:
  LogLevel m_level;
  std::string m_component;
  bool m_enabled;
  std::ostringstream m_stream;
};

inline LogLine log(LogLevel level, std::string_view component) {
  return LogLine(level, component);
}

} // namespace nodo::utils

#endif
