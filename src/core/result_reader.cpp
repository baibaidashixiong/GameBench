#include "core/result_reader.hpp"

#include <charconv>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace gamebench::core {
namespace {

[[nodiscard]] auto find_key(std::string_view json, std::string_view key,
                            std::size_t start = 0) -> std::size_t {
  std::string quoted = "\"" + std::string{key} + "\"";
  return json.find(quoted, start);
}

[[nodiscard]] auto value_start(std::string_view json, std::string_view key,
                               std::size_t start = 0) -> std::size_t {
  const auto key_position = find_key(json, key, start);
  if (key_position == std::string_view::npos) {
    throw std::runtime_error("结果文件缺少字段: " + std::string{key});
  }
  const auto colon = json.find(':', key_position);
  if (colon == std::string_view::npos) {
    throw std::runtime_error("结果文件字段格式无效: " + std::string{key});
  }
  auto position = colon + 1;
  while (position < json.size() && (json[position] == ' ' || json[position] == '\n' ||
                                    json[position] == '\r' || json[position] == '\t')) {
    ++position;
  }
  return position;
}

[[nodiscard]] auto read_string(std::string_view json, std::string_view key,
                               std::size_t start = 0) -> std::string {
  auto position = value_start(json, key, start);
  if (position >= json.size() || json[position] != '"') {
    throw std::runtime_error("结果文件字符串字段无效: " + std::string{key});
  }
  const auto end = json.find('"', position + 1);
  if (end == std::string_view::npos) {
    throw std::runtime_error("结果文件字符串未闭合: " + std::string{key});
  }
  return std::string{json.substr(position + 1, end - position - 1)};
}

[[nodiscard]] auto read_number(std::string_view json, std::string_view key,
                               std::size_t start = 0) -> double {
  const auto position = value_start(json, key, start);
  double value = 0.0;
  const auto result = std::from_chars(json.data() + position, json.data() + json.size(), value);
  if (result.ec != std::errc{}) {
    throw std::runtime_error("结果文件数值字段无效: " + std::string{key});
  }
  return value;
}

} // namespace

auto read_result_summary(const std::filesystem::path& path) -> ResultSummary {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("无法读取结果文件: " + path.string());
  }
  const std::string contents{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};

  ResultSummary summary;
  summary.benchmark_id = read_string(contents, "benchmark_id");
  summary.valid = read_string(contents, "validation_status") == "passed";

  const auto scaling_position = find_key(contents, "thread_scaling");
  const auto end = scaling_position == std::string::npos ? contents.size() : scaling_position;
  std::size_t position = 0;
  while (true) {
    const auto thread_key = find_key(contents, "thread_count", position);
    if (thread_key == std::string::npos || thread_key >= end) {
      break;
    }
    const auto thread_count = static_cast<unsigned>(
        read_number(contents, "thread_count", thread_key));
    const auto median = read_number(contents, "median_frame_time_ms", thread_key);
    summary.median_by_thread_count.emplace_back(thread_count, median);
    position = thread_key + 1;
  }

  if (summary.median_by_thread_count.empty()) {
    throw std::runtime_error("结果文件没有 timing_results 数据");
  }
  return summary;
}

} // namespace gamebench::core

