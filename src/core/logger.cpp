#include "core/logger.hpp"

#include <iostream>
#include <string>

namespace gamebench::core {
namespace {

[[nodiscard]] auto level_name(LogLevel level) -> std::string_view {
  switch (level) {
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warning:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  }
  return "UNKNOWN";
}

} // namespace

Logger::Logger(LogLevel minimum_level) : minimum_level_(minimum_level) {}

void Logger::debug(std::string_view message) { log(LogLevel::Debug, message); }

void Logger::info(std::string_view message) { log(LogLevel::Info, message); }

void Logger::warning(std::string_view message) { log(LogLevel::Warning, message); }

void Logger::error(std::string_view message) { log(LogLevel::Error, message); }

void Logger::log(LogLevel level, std::string_view message) {
  if (level < minimum_level_) {
    return;
  }

  const std::scoped_lock lock(output_mutex_);
  std::cerr << "[GameBench][" << level_name(level) << "] " << message << '\n';
}

} // namespace gamebench::core

