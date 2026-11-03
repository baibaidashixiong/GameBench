#pragma once

#include "core/runner.hpp"

#include <filesystem>
#include <vector>

namespace gamebench::core {

class ResultManager final {
public:
  static void write(const BenchmarkResult& result, const std::filesystem::path& path,
                    ExportFormat format);
  [[nodiscard]] static auto default_output_path(const BenchmarkResult& result, ExportFormat format)
      -> std::filesystem::path;
};

} // namespace gamebench::core
