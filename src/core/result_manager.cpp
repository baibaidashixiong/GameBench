#include "core/result_manager.hpp"

#include "benchmark/scoring.hpp"

#include <algorithm>
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

void write_json_unsigned_array(std::ostream& output, const std::vector<unsigned>& values) {
  output << '[';
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << values[index];
  }
  output << ']';
}

void write_json(const BenchmarkResult& result, std::ostream& output) {
  const bool all_skipped =
      std::ranges::all_of(result.runs, [](const BenchmarkRun& run) { return run.skipped; });
  const auto top_status = all_skipped ? "skipped" : (result.valid ? "passed" : "failed");
  output << std::fixed << std::setprecision(9);
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"benchmark_id\": ";
  write_escaped_json(output, result.benchmark_id);
  output << ",\n  \"benchmark_version\": ";
  write_escaped_json(output, result.benchmark_version);
  output << ",\n  \"backend\": ";
  write_escaped_json(output, result.backend);
  output << ",\n  \"validation_status\": \"" << top_status << '"' << ",\n  \"configuration\": {\n"
         << "    \"workload\": ";
  write_escaped_json(output, result.config.workload);
  output << ",\n    \"profile\": ";
  write_escaped_json(output, result.config.profile);
  output << ",\n    \"mode\": ";
  write_escaped_json(output, to_string(result.config.mode));
  output << ",\n    \"scheduler\": ";
  write_escaped_json(output, to_string(result.config.scheduler));
  output << ",\n    \"layout\": ";
  write_escaped_json(output, to_string(result.config.layout));
  output << ",\n    \"memory_pattern\": ";
  write_escaped_json(output, to_string(result.config.memory_pattern));
  output << ",\n    \"vulkan_mode\": ";
  write_escaped_json(output, to_string(result.config.vulkan_mode));
  output << ",\n    \"seed\": " << result.config.seed
         << ",\n    \"objects\": " << result.config.objects
         << ",\n    \"working_set_bytes\": " << result.config.working_set_bytes
         << ",\n    \"audio_buffer_size\": " << result.config.audio_buffer_size
         << ",\n    \"sample_rate\": " << result.config.sample_rate << ",\n    \"cpu_affinity\": ";
  write_json_unsigned_array(output, result.config.cpu_affinity);
  output << ",\n    \"hardware_counters\": " << (result.config.hardware_counters ? "true" : "false")
         << ",\n    \"iterations\": " << result.config.iterations
         << ",\n    \"warmup_iterations\": " << result.config.warmup_iterations
         << ",\n    \"repetitions\": " << result.config.repetitions << "\n  },\n";
  if (result.frame_profile) {
    const auto& profile = *result.frame_profile;
    output << "  \"frame_profile\": {\n    \"id\": ";
    write_escaped_json(output, profile.id);
    output << ",\n    \"version\": ";
    write_escaped_json(output, profile.version);
    output << ",\n    \"ecs_entities\": " << profile.ecs_entities
           << ",\n    \"scene_sets\": " << profile.scene_sets
           << ",\n    \"scene_objects\": " << profile.scene_objects
           << ",\n    \"scene_views\": " << profile.scene_views
           << ",\n    \"physics_bodies\": " << profile.physics_bodies
           << ",\n    \"physics_substeps\": " << profile.physics_substeps
           << ",\n    \"animation_skeletons\": " << profile.animation_skeletons
           << ",\n    \"bones_per_skeleton\": " << profile.bones_per_skeleton
           << ",\n    \"ai_agents\": " << profile.ai_agents
           << ",\n    \"simd_particles\": " << profile.simd_particles
           << ",\n    \"simd_steps\": " << profile.simd_steps
           << ",\n    \"audio_sources\": " << profile.audio_sources
           << ",\n    \"audio_buffer_size\": " << profile.audio_buffer_size
           << ",\n    \"sample_rate\": " << profile.sample_rate
           << ",\n    \"vulkan_tasks\": " << profile.vulkan_tasks
           << ",\n    \"vulkan_commands_per_task\": " << profile.vulkan_commands_per_task
           << "\n  },\n";
  }
  output << "  \"hardware\": {\n    \"cpu_vendor\": ";
  write_escaped_json(output, result.system.cpu_vendor);
  output << ",\n    \"cpu_model\": ";
  write_escaped_json(output, result.system.cpu_model);
  output << ",\n    \"logical_processors\": " << result.system.logical_processors
         << ",\n    \"physical_cores\": " << result.topology.physical_cores
         << ",\n    \"smt_enabled\": " << (result.topology.smt_enabled ? "true" : "false")
         << ",\n    \"numa_nodes\": " << result.topology.numa_nodes << ",\n    \"allowed_cpus\": ";
  write_json_unsigned_array(output, result.topology.allowed_cpus);
  output << ",\n    \"ccd_type\": ";
  write_escaped_json(output, result.topology.ccd_type);
  output << ",\n    \"core_type\": ";
  write_escaped_json(output, result.topology.core_type);
  output << ",\n    \"caches\": [";
  for (std::size_t index = 0; index < result.topology.caches.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& cache = result.topology.caches[index];
    output << "{\"level\": " << cache.level << ", \"type\": ";
    write_escaped_json(output, cache.type);
    output << ", \"size_bytes\": " << cache.size_bytes << '}';
  }
  output << ']' << ",\n    \"cpu_features\": ";
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
  if (result.system.current_frequency_mhz) {
    output << ",\n    \"current_frequency_mhz\": " << *result.system.current_frequency_mhz;
  }
  if (result.system.minimum_frequency_mhz) {
    output << ",\n    \"minimum_frequency_mhz\": " << *result.system.minimum_frequency_mhz;
  }
  if (result.system.maximum_frequency_mhz) {
    output << ",\n    \"maximum_frequency_mhz\": " << *result.system.maximum_frequency_mhz;
  }
  if (result.system.package_temperature_celsius) {
    output << ",\n    \"package_temperature_celsius\": "
           << *result.system.package_temperature_celsius;
  }
  output << "\n  }";
  if (result.hardware_counters) {
    const auto& counters = *result.hardware_counters;
    output << ",\n  \"hardware_counters\": {\n    \"requested\": "
           << (counters.requested ? "true" : "false")
           << ",\n    \"available\": " << (counters.available ? "true" : "false")
           << ",\n    \"cycles\": " << counters.cycles
           << ",\n    \"instructions\": " << counters.instructions
           << ",\n    \"ipc\": " << counters.ipc
           << ",\n    \"branch_misses\": " << counters.branch_misses
           << ",\n    \"cache_misses\": " << counters.cache_misses
           << ",\n    \"context_switches\": " << counters.context_switches << ",\n    \"scope\": ";
    write_escaped_json(output, counters.scope);
    output << ",\n    \"message\": ";
    write_escaped_json(output, counters.message);
    output << "\n  }";
  }
  output << ",\n  \"timing_results\": [\n";

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
           << ",\n      \"critical_path_time_ms\": " << run.critical_path_time_ms
           << ",\n      \"main_thread_time_ms\": " << run.main_thread_time_ms
           << ",\n      \"main_thread_cpu_migrations\": " << run.main_thread_cpu_migrations;
    if (run.worker_thread_time_ms) {
      output << ",\n      \"worker_thread_time_ms\": " << *run.worker_thread_time_ms;
    }
    if (run.total_cpu_work_ms) {
      output << ",\n      \"total_cpu_work_ms\": " << *run.total_cpu_work_ms;
    }
    output << ",\n      \"operations\": " << run.operations
           << ",\n      \"checksum\": " << run.checksum << ",\n      \"validation_status\": "
           << (run.skipped ? "\"skipped\"" : (run.valid ? "\"passed\"" : "\"failed\""))
           << ",\n      \"validation_message\": ";
    write_escaped_json(output, run.validation_message);
    if (!run.warnings.empty()) {
      output << ",\n      \"warnings\": ";
      write_json_string_array(output, run.warnings);
    }
    if (run.scheduling) {
      output << ",\n      \"scheduling\": {\n        \"submitted_jobs\": "
             << run.scheduling->submitted_jobs
             << ",\n        \"completed_jobs\": " << run.scheduling->completed_jobs
             << ",\n        \"stolen_jobs\": " << run.scheduling->stolen_jobs
             << ",\n        \"main_thread_jobs\": " << run.scheduling->main_thread_jobs
             << ",\n        \"exceptions\": " << run.scheduling->exceptions
             << ",\n        \"worker_busy_ms\": " << run.scheduling->worker_busy_ms << "\n      }";
    }
    if (run.vulkan) {
      output << ",\n      \"vulkan\": {\n        \"device_name\": ";
      write_escaped_json(output, run.vulkan->device_name);
      output << ",\n        \"driver_name\": ";
      write_escaped_json(output, run.vulkan->driver_name);
      output << ",\n        \"api_version\": " << run.vulkan->api_version << "\n      }";
    }
    if (!run.simd_backend.empty()) {
      output << ",\n      \"simd_backend\": ";
      write_escaped_json(output, run.simd_backend);
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
  output << "\n  ],\n  \"scoring\": {\n    \"scoring_version\": 1,\n"
            "    \"comparable_across_workloads\": false,\n    \"note\": ";
  const auto scores = benchmark::calculate_scores(result);
  write_escaped_json(output, scores.note);
  output << ",\n    \"points\": [";
  for (std::size_t index = 0; index < scores.points.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& point = scores.points[index];
    output << "{\"thread_count\": " << point.thread_count
           << ", \"throughput_operations_per_second\": " << point.throughput_operations_per_second
           << ", \"thread_scaling_score\": " << point.thread_scaling_score << '}';
  }
  output << "]\n  }\n}\n";
}

void write_csv(const BenchmarkResult& result, std::ostream& output) {
  output << "benchmark_id,benchmark_version,thread_count,mean_frame_time_ms,"
            "median_frame_time_ms,p95_frame_time_ms,p99_frame_time_ms,p99_9_frame_time_ms,"
            "maximum_frame_time_ms,critical_path_time_ms,main_thread_time_ms,"
            "main_thread_cpu_migrations,speedup,efficiency,operations,checksum,"
            "validation_status\n";
  output << std::fixed << std::setprecision(9);
  for (std::size_t index = 0; index < result.runs.size(); ++index) {
    const auto& run = result.runs[index];
    const auto& scaling = result.thread_scaling[index];
    output << result.benchmark_id << ',' << result.benchmark_version << ',' << run.thread_count
           << ',' << run.statistics.mean_ms << ',' << run.statistics.median_ms << ','
           << run.statistics.p95_ms << ',' << run.statistics.p99_ms << ',' << run.statistics.p999_ms
           << ',' << run.statistics.maximum_ms << ',' << run.critical_path_time_ms << ','
           << run.main_thread_time_ms << ',' << run.main_thread_cpu_migrations << ','
           << scaling.speedup << ',' << scaling.efficiency << ',' << run.operations << ','
           << run.checksum << ',' << (run.skipped ? "skipped" : (run.valid ? "passed" : "failed"))
           << '\n';
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
