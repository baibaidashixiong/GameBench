#pragma once

#include <mutex>
#include <string_view>

namespace gamebench::core {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger final {
public:
  explicit Logger(LogLevel minimum_level = LogLevel::Info);

  void debug(std::string_view message);
  void info(std::string_view message);
  void warning(std::string_view message);
  void error(std::string_view message);

private:
  void log(LogLevel level, std::string_view message);

  LogLevel minimum_level_;
  std::mutex output_mutex_;
};

} // namespace gamebench::core
