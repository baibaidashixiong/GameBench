#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace gamebench::core {

struct ResultSummary {
  std::string benchmark_id;
  bool valid{};
  std::vector<std::pair<unsigned, double>> median_by_thread_count;
};

[[nodiscard]] auto read_result_summary(const std::filesystem::path& path) -> ResultSummary;

} // namespace gamebench::core
