#include "core/result_manager.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gamebench::core {
namespace {

void write_escaped_json(std::ostream& output, std::string_view value) {
  output << '"';
  for (const char character : value) {
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      output << character;
      break;
    }
  }
  output << '"';
}

void write_json_string_array(std::ostream& output, const std::vector<std::string>& values) {
  output << '[';
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    write_escaped_json(output, values[index]);
  }
  output << ']';
}

void write_json(const BenchmarkResult& result, std::ostream& output) {
  output << std::fixed << std::setprecision(9);
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"benchmark_id\": ";
  write_escaped_json(output, result.benchmark_id);
  output << ",\n  \"benchmark_version\": ";
  write_escaped_json(output, result.benchmark_version);
  output << ",\n  \"backend\": ";
  write_escaped_json(output, result.backend);
  output << ",\n  \"validation_status\": " << (result.valid ? "\"passed\"" : "\"failed\"")
         << ",\n  \"configuration\": {\n"
         << "    \"workload\": ";
  write_escaped_json(output, result.config.workload);
  output << ",\n    \"mode\": ";
  write_escaped_json(output, to_string(result.config.mode));
  output << ",\n    \"scheduler\": ";
  write_escaped_json(output, to_string(result.config.scheduler));
  output << ",\n    \"seed\": " << result.config.seed
         << ",\n    \"objects\": " << result.config.objects
         << ",\n    \"iterations\": " << result.config.iterations
         << ",\n    \"warmup_iterations\": " << result.config.warmup_iterations
         << ",\n    \"repetitions\": " << result.config.repetitions << "\n  },\n"
         << "  \"hardware\": {\n    \"cpu_vendor\": ";
  write_escaped_json(output, result.system.cpu_vendor);
  output << ",\n    \"cpu_model\": ";
  write_escaped_json(output, result.system.cpu_model);
  output << ",\n    \"logical_processors\": " << result.system.logical_processors
         << ",\n    \"cpu_features\": ";
  write_json_string_array(output, result.system.cpu_features);
  output << "\n  },\n  \"environment\": {\n    \"operating_system\": ";
  write_escaped_json(output, result.system.operating_system);
  output << ",\n    \"kernel_release\": ";
  write_escaped_json(output, result.system.kernel_release);
  output << ",\n    \"compiler\": ";
  write_escaped_json(output, result.system.compiler);
  output << ",\n    \"compiler_version\": ";
  write_escaped_json(output, result.system.compiler_version);
  output << ",\n    \"build_type\": ";
  write_escaped_json(output, result.system.build_type);
  output << ",\n    \"cpu_governor\": ";
  write_escaped_json(output, result.system.cpu_governor);
  output << "\n  },\n  \"timing_results\": [\n";

  for (std::size_t run_index = 0; run_index < result.runs.size(); ++run_index) {
    const auto& run = result.runs[run_index];
    if (run_index != 0) {
      output << ",\n";
    }
    output << "    {\n      \"thread_count\": " << run.thread_count << ",\n      \"samples_ms\": [";
    for (std::size_t sample_index = 0; sample_index < run.samples_ms.size(); ++sample_index) {
      if (sample_index != 0) {
        output << ", ";
      }
      output << run.samples_ms[sample_index];
    }
    output << "],\n      \"mean_frame_time_ms\": " << run.statistics.mean_ms
           << ",\n      \"median_frame_time_ms\": " << run.statistics.median_ms
           << ",\n      \"p95_frame_time_ms\": " << run.statistics.p95_ms
           << ",\n      \"p99_frame_time_ms\": " << run.statistics.p99_ms
           << ",\n      \"p99_9_frame_time_ms\": " << run.statistics.p999_ms
           << ",\n      \"maximum_frame_time_ms\": " << run.statistics.maximum_ms
           << ",\n      \"operations\": " << run.operations
           << ",\n      \"checksum\": " << run.checksum
           << ",\n      \"validation_status\": " << (run.valid ? "\"passed\"" : "\"failed\"")
           << ",\n      \"validation_message\": ";
    write_escaped_json(output, run.validation_message);
    if (run.scheduling) {
      output << ",\n      \"scheduling\": {\n        \"submitted_jobs\": "
             << run.scheduling->submitted_jobs
             << ",\n        \"completed_jobs\": " << run.scheduling->completed_jobs
             << ",\n        \"stolen_jobs\": " << run.scheduling->stolen_jobs
             << ",\n        \"main_thread_jobs\": " << run.scheduling->main_thread_jobs
             << ",\n        \"exceptions\": " << run.scheduling->exceptions
             << ",\n        \"worker_busy_ms\": " << run.scheduling->worker_busy_ms << "\n      }";
    }
    output << "\n    }";
  }

  output << "\n  ],\n  \"thread_scaling\": [\n";
  for (std::size_t index = 0; index < result.thread_scaling.size(); ++index) {
    const auto& scaling = result.thread_scaling[index];
    if (index != 0) {
      output << ",\n";
    }
    output << "    {\n      \"thread_count\": " << scaling.thread_count
           << ",\n      \"speedup\": " << scaling.speedup
           << ",\n      \"efficiency\": " << scaling.efficiency << "\n    }";
  }
  output << "\n  ]\n}\n";
}

void write_csv(const BenchmarkResult& result, std::ostream& output) {
  output << "benchmark_id,benchmark_version,thread_count,mean_frame_time_ms,"
            "median_frame_time_ms,p95_frame_time_ms,p99_frame_time_ms,p99_9_frame_time_ms,"
            "maximum_frame_time_ms,speedup,efficiency,operations,checksum,validation_status\n";
  output << std::fixed << std::setprecision(9);
  for (std::size_t index = 0; index < result.runs.size(); ++index) {
    const auto& run = result.runs[index];
    const auto& scaling = result.thread_scaling[index];
    output << result.benchmark_id << ',' << result.benchmark_version << ',' << run.thread_count
           << ',' << run.statistics.mean_ms << ',' << run.statistics.median_ms << ','
           << run.statistics.p95_ms << ',' << run.statistics.p99_ms << ',' << run.statistics.p999_ms
           << ',' << run.statistics.maximum_ms << ',' << scaling.speedup << ','
           << scaling.efficiency << ',' << run.operations << ',' << run.checksum << ','
           << (run.valid ? "passed" : "failed") << '\n';
  }
}

[[nodiscard]] auto timestamp() -> std::string {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
  localtime_r(&time, &local_time);
  std::ostringstream output;
  output << std::put_time(&local_time, "%Y%m%d-%H%M%S");
  return output.str();
}

} // namespace

void ResultManager::write(const BenchmarkResult& result, const std::filesystem::path& path,
                          ExportFormat format) {
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("无法写入结果文件: " + path.string());
  }
  if (format == ExportFormat::Json) {
    write_json(result, output);
  } else {
    write_csv(result, output);
  }
}

auto ResultManager::default_output_path(const BenchmarkResult& result, ExportFormat format)
    -> std::filesystem::path {
  const auto extension = format == ExportFormat::Json ? ".json" : ".csv";
  return std::filesystem::path{"results"} / (result.benchmark_id + "-" + timestamp() + extension);
}

} // namespace gamebench::core
