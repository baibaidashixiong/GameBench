#include "core/cli.hpp"

#include "benchmark/engine_adapter.hpp"
#include "benchmark/frame_profile.hpp"
#include "benchmark/registry.hpp"
#include "core/logger.hpp"
#include "core/result_manager.hpp"
#include "core/result_reader.hpp"
#include "core/runner.hpp"
#include "core/version.hpp"
#include "platform/cpu_topology.hpp"
#include "platform/system_info.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace gamebench::core {
namespace {

template<typename Integer>
[[nodiscard]] auto parse_integer(std::string_view text, std::string_view option) -> Integer {
  Integer value{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
    throw std::invalid_argument("参数 " + std::string{option} + " 的值无效: " + std::string{text});
  }
  return value;
}

[[nodiscard]] auto parse_threads(std::string_view text) -> std::vector<unsigned> {
  std::vector<unsigned> threads;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto comma = text.find(',', start);
    const auto end = comma == std::string_view::npos ? text.size() : comma;
    const auto token = text.substr(start, end - start);
    if (token.empty()) {
      throw std::invalid_argument("--threads 包含空值");
    }
    threads.push_back(parse_integer<unsigned>(token, "--threads"));
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return threads;
}

[[nodiscard]] auto option_value(std::span<const std::string_view> arguments, std::size_t& index)
    -> std::string_view {
  if (index + 1 >= arguments.size()) {
    throw std::invalid_argument("参数 " + std::string{arguments[index]} + " 缺少值");
  }
  ++index;
  return arguments[index];
}

void print_help() {
  std::cout << "GameBench " << kVersion << "\n"
            << "面向 Linux 的游戏引擎 CPU 性能测试平台\n\n"
            << "用法:\n"
            << "  gamebench run [选项]\n"
            << "  gamebench list\n"
            << "  gamebench system-info\n"
            << "  gamebench engine-info\n"
            << "  gamebench profile list\n"
            << "  gamebench compare <基准结果.json> <对比结果.json>\n"
            << "  gamebench --version\n\n"
            << "run 选项:\n"
            << "  --workload <id>          工作负载 ID\n"
            << "  --profile <id>           选择单帧合成负载 Profile\n"
            << "                           默认: source2-like + 自动线程梯度\n"
            << "  --suite all              运行全部已实现工作负载\n"
            << "  --threads <1,2,4>        总执行参与者数（含主线程）\n"
            << "  --mode <reproducible|realistic>\n"
            << "  --mode <no-submit|gpu-backed> Vulkan 执行模式\n"
            << "  --scheduler <global-queue|static-partitioning|work-stealing>\n"
            << "  --layout <aos|soa|aosoa> ECS 数据布局\n"
            << "  --pattern <sequential|random|pointer-chasing|strided> 内存访问模式\n"
            << "  --working-set <字节>     内存工作集大小\n"
            << "  --buffer-size <帧数>     Audio DSP 缓冲区大小\n"
            << "  --sample-rate <Hz>       Audio DSP 输出采样率\n"
            << "  --cpu-affinity <列表>    绑定主线程与 Worker 的逻辑 CPU\n"
            << "  --physical-core <列表>   按物理核心索引选择逻辑 CPU\n"
            << "  --numa-node <列表>       按 NUMA 节点索引选择逻辑 CPU\n"
            << "  --hardware-counters      尝试采集 perf 硬件计数器\n"
            << "  --seed <整数>            固定随机种子\n"
            << "  --objects <数量>         工作负载规模\n"
            << "  --iterations <数量>      每个样本内的工作负载迭代数\n"
            << "  --warmup <数量>          预热迭代数\n"
            << "  --repetitions <数量>     采样重复数\n"
            << "  --format <json|csv>      输出格式\n"
            << "  --output <路径>          输出文件\n";
}

[[nodiscard]] auto parse_mode(std::string_view value) -> RunMode {
  if (value == "reproducible") {
    return RunMode::Reproducible;
  }
  if (value == "realistic") {
    return RunMode::Realistic;
  }
  throw std::invalid_argument("--mode 仅支持 reproducible 或 realistic");
}

[[nodiscard]] auto parse_scheduler(std::string_view value) -> SchedulerKind {
  if (value == "global-queue") {
    return SchedulerKind::GlobalQueue;
  }
  if (value == "static-partitioning") {
    return SchedulerKind::StaticPartitioning;
  }
  if (value == "work-stealing") {
    return SchedulerKind::WorkStealing;
  }
  throw std::invalid_argument("--scheduler 的值无效");
}

[[nodiscard]] auto parse_format(std::string_view value) -> ExportFormat {
  if (value == "json") {
    return ExportFormat::Json;
  }
  if (value == "csv") {
    return ExportFormat::Csv;
  }
  throw std::invalid_argument("--format 仅支持 json 或 csv");
}

[[nodiscard]] auto parse_layout(std::string_view value) -> DataLayout {
  if (value == "aos") {
    return DataLayout::AoS;
  }
  if (value == "soa") {
    return DataLayout::SoA;
  }
  if (value == "aosoa") {
    return DataLayout::AoSoA;
  }
  throw std::invalid_argument("--layout 仅支持 aos、soa 或 aosoa");
}

[[nodiscard]] auto parse_memory_pattern(std::string_view value) -> MemoryPattern {
  if (value == "sequential") {
    return MemoryPattern::Sequential;
  }
  if (value == "random") {
    return MemoryPattern::Random;
  }
  if (value == "pointer-chasing") {
    return MemoryPattern::PointerChasing;
  }
  if (value == "strided") {
    return MemoryPattern::Strided;
  }
  throw std::invalid_argument("--pattern 的值无效");
}

[[nodiscard]] auto parse_vulkan_mode(std::string_view value) -> VulkanMode {
  if (value == "no-submit") {
    return VulkanMode::NoSubmit;
  }
  if (value == "gpu-backed") {
    return VulkanMode::GpuBacked;
  }
  throw std::invalid_argument("Vulkan --mode 仅支持 no-submit 或 gpu-backed");
}

void print_system_info() {
  const auto info = platform::collect_system_info();
  const auto topology = platform::detect_cpu_topology();
  std::cout << "CPU 厂商: " << info.cpu_vendor << '\n'
            << "CPU 型号: " << info.cpu_model << '\n'
            << "逻辑处理器: " << info.logical_processors << '\n'
            << "物理核心: " << topology.physical_cores << '\n'
            << "SMT: " << (topology.smt_enabled ? "启用" : "未启用或 Unknown") << '\n'
            << "NUMA 节点: " << topology.numa_nodes << '\n'
            << "CCD 类型: " << topology.ccd_type << '\n'
            << "核心类型: " << topology.core_type << '\n'
            << "操作系统: " << info.operating_system << '\n'
            << "内核: " << info.kernel_release << '\n'
            << "编译器: " << info.compiler << ' ' << info.compiler_version << '\n'
            << "构建类型: " << info.build_type << '\n'
            << "CPU Governor: " << info.cpu_governor << '\n'
            << "可用逻辑 CPU: ";
  for (std::size_t index = 0; index < topology.allowed_cpus.size(); ++index) {
    if (index != 0) {
      std::cout << ',';
    }
    std::cout << topology.allowed_cpus[index];
  }
  std::cout << '\n';
  for (const auto& cache : topology.caches) {
    std::cout << "缓存: L" << cache.level << ' ' << cache.type << ' ' << cache.size_bytes / 1024U
              << " KiB\n";
  }
  if (info.current_frequency_mhz) {
    std::cout << "当前频率: " << *info.current_frequency_mhz << " MHz\n";
  }
  if (info.maximum_frequency_mhz) {
    std::cout << "最大频率: " << *info.maximum_frequency_mhz << " MHz\n";
  }
  if (info.package_temperature_celsius) {
    std::cout << "封装温度: " << *info.package_temperature_celsius << " C\n";
  } else {
    std::cout << "封装温度: Unavailable\n";
  }
}

void print_workloads(const benchmark::WorkloadRegistry& registry) {
  std::cout << std::left << std::setw(18) << "ID" << std::setw(10) << "版本" << std::setw(14)
            << "类别" << "说明\n";
  for (const auto& descriptor : registry.descriptors()) {
    std::cout << std::left << std::setw(18) << descriptor.id << std::setw(10) << descriptor.version
              << std::setw(14) << descriptor.category << descriptor.description << '\n';
  }
}

void print_engine_adapters() {
  for (const auto& adapter : benchmark::planned_engine_adapters()) {
    std::string status;
    switch (adapter.availability) {
    case benchmark::EngineAvailability::Available:
      status = "available";
      break;
    case benchmark::EngineAvailability::DetectedOnly:
      status = "detected-only";
      break;
    case benchmark::EngineAvailability::MissingDependency:
      status = "missing";
      break;
    case benchmark::EngineAvailability::NotImplemented:
      status = "not-implemented";
      break;
    }
    std::cout << std::left << std::setw(14) << adapter.id << std::setw(18) << adapter.display_name
              << std::setw(16) << status << adapter.message << '\n';
  }
}

void print_frame_profiles() {
  std::cout << std::left << std::setw(18) << "ID" << std::setw(10) << "版本" << "说明\n";
  for (const auto& profile : benchmark::available_frame_profiles()) {
    std::cout << std::left << std::setw(18) << profile.id << std::setw(10) << profile.version
              << profile.description << '\n';
  }
}

void print_result(const BenchmarkResult& result, const std::filesystem::path& output) {
  std::cout << "工作负载: " << result.benchmark_id << '\n';
  for (const auto& run : result.runs) {
    std::cout << "  线程 " << run.thread_count << ": 中位 " << std::fixed << std::setprecision(3)
              << run.statistics.median_ms << " ms, P99 " << run.statistics.p99_ms << " ms, 校验 "
              << (run.skipped ? "跳过" : (run.valid ? "通过" : "失败")) << '\n';
  }
  std::cout << "结果: " << output << '\n';
}

[[nodiscard]] auto find_median(const ResultSummary& summary, unsigned threads) -> double {
  const auto match =
      std::ranges::find_if(summary.median_by_thread_count,
                           [threads](const auto& item) { return item.first == threads; });
  if (match == summary.median_by_thread_count.end()) {
    throw std::runtime_error("结果缺少线程数 " + std::to_string(threads) + " 的样本");
  }
  return match->second;
}

auto compare_results(const std::filesystem::path& baseline_path,
                     const std::filesystem::path& candidate_path) -> int {
  const auto baseline = read_result_summary(baseline_path);
  const auto candidate = read_result_summary(candidate_path);
  if (baseline.benchmark_id != candidate.benchmark_id) {
    throw std::runtime_error("不能比较不同工作负载的结果");
  }
  if (!baseline.valid || !candidate.valid) {
    throw std::runtime_error("不能比较未通过校验的结果");
  }

  std::cout << "工作负载: " << baseline.benchmark_id << '\n'
            << std::right << std::setw(10) << "线程" << std::setw(18) << "基准中位(ms)"
            << std::setw(18) << "对比中位(ms)" << std::setw(16) << "相对吞吐" << '\n';
  std::cout << std::fixed << std::setprecision(3);
  for (const auto& [threads, baseline_median] : baseline.median_by_thread_count) {
    const auto candidate_median = find_median(candidate, threads);
    const auto relative = baseline_median / candidate_median;
    std::cout << std::right << std::setw(10) << threads << std::setw(18) << baseline_median
              << std::setw(18) << candidate_median << std::setw(15) << relative << "x\n";
  }
  return EXIT_SUCCESS;
}

auto run_command(std::span<const std::string_view> arguments) -> int {
  auto config = parse_run_config(arguments);
  if (config.thread_counts.empty()) {
    const auto physical_cores = platform::detect_cpu_topology().physical_cores;
    config.thread_counts.push_back(1);
    for (const auto candidate : {2U, 4U, 8U, 16U}) {
      if (candidate <= physical_cores && candidate != config.thread_counts.back()) {
        config.thread_counts.push_back(candidate);
      }
    }
    if (physical_cores > config.thread_counts.back()) {
      config.thread_counts.push_back(physical_cores);
    }
  }
  const auto registry = benchmark::WorkloadRegistry::create_default();

  std::vector<std::string> workload_ids;
  if (config.suite_all) {
    for (const auto& descriptor : registry.descriptors()) {
      workload_ids.push_back(descriptor.id);
    }
  } else {
    workload_ids.push_back(config.workload);
  }

  if (!config.output_path.empty() && workload_ids.size() != 1) {
    throw std::invalid_argument("--suite all 不支持单一 --output 路径");
  }

  for (const auto& workload_id : workload_ids) {
    auto workload = registry.create(workload_id);
    if (!workload) {
      throw std::invalid_argument("未知工作负载: " + workload_id + "，使用 list 查看可用项");
    }

    std::cout << "运行 " << workload_id << "，线程数 ";
    for (std::size_t index = 0; index < config.thread_counts.size(); ++index) {
      if (index != 0) {
        std::cout << ',';
      }
      std::cout << config.thread_counts[index];
    }
    std::cout << " ...\n";

    auto workload_config = config;
    workload_config.workload = workload_id;
    const auto result = BenchmarkRunner{}.run(*workload, workload_config);
    const auto output = workload_config.output_path.empty()
                            ? ResultManager::default_output_path(result, workload_config.format)
                            : workload_config.output_path;
    ResultManager::write(result, output, workload_config.format);
    print_result(result, output);
    if (!result.valid) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}

} // namespace

auto parse_run_config(std::span<const std::string_view> arguments) -> RunConfig {
  RunConfig config;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const auto option = arguments[index];
    if (option == "--workload") {
      config.workload = option_value(arguments, index);
      config.suite_all = false;
    } else if (option == "--profile") {
      config.profile = option_value(arguments, index);
      config.workload = "game-frame";
      config.suite_all = false;
    } else if (option == "--suite") {
      const auto suite = option_value(arguments, index);
      if (suite != "all") {
        throw std::invalid_argument("--suite 当前仅支持 all");
      }
      config.suite_all = true;
    } else if (option == "--threads") {
      config.thread_counts = parse_threads(option_value(arguments, index));
    } else if (option == "--mode") {
      const auto mode = option_value(arguments, index);
      if (mode == "no-submit" || mode == "gpu-backed") {
        config.vulkan_mode = parse_vulkan_mode(mode);
      } else {
        config.mode = parse_mode(mode);
      }
    } else if (option == "--scheduler") {
      config.scheduler = parse_scheduler(option_value(arguments, index));
    } else if (option == "--layout") {
      config.layout = parse_layout(option_value(arguments, index));
    } else if (option == "--pattern") {
      config.memory_pattern = parse_memory_pattern(option_value(arguments, index));
    } else if (option == "--working-set") {
      config.working_set_bytes =
          parse_integer<std::uint64_t>(option_value(arguments, index), option);
    } else if (option == "--buffer-size") {
      config.audio_buffer_size =
          parse_integer<std::uint32_t>(option_value(arguments, index), option);
    } else if (option == "--sample-rate") {
      config.sample_rate = parse_integer<std::uint32_t>(option_value(arguments, index), option);
    } else if (option == "--cpu-affinity") {
      config.cpu_affinity = parse_threads(option_value(arguments, index));
    } else if (option == "--physical-core") {
      const auto indices = parse_threads(option_value(arguments, index));
      for (const auto core_index : indices) {
        const auto cpus = platform::logical_cpus_for_physical_core(core_index);
        config.cpu_affinity.insert(config.cpu_affinity.end(), cpus.begin(), cpus.end());
      }
      std::ranges::sort(config.cpu_affinity);
      config.cpu_affinity.erase(std::ranges::unique(config.cpu_affinity).begin(),
                                config.cpu_affinity.end());
    } else if (option == "--numa-node") {
      const auto indices = parse_threads(option_value(arguments, index));
      for (const auto node_index : indices) {
        const auto cpus = platform::logical_cpus_for_numa_node(node_index);
        config.cpu_affinity.insert(config.cpu_affinity.end(), cpus.begin(), cpus.end());
      }
      std::ranges::sort(config.cpu_affinity);
      config.cpu_affinity.erase(std::ranges::unique(config.cpu_affinity).begin(),
                                config.cpu_affinity.end());
    } else if (option == "--hardware-counters") {
      config.hardware_counters = true;
    } else if (option == "--seed") {
      config.seed = parse_integer<std::uint64_t>(option_value(arguments, index), option);
    } else if (option == "--objects") {
      config.objects = parse_integer<std::uint64_t>(option_value(arguments, index), option);
    } else if (option == "--iterations") {
      config.iterations = parse_integer<std::uint64_t>(option_value(arguments, index), option);
    } else if (option == "--warmup") {
      config.warmup_iterations =
          parse_integer<std::uint64_t>(option_value(arguments, index), option);
    } else if (option == "--repetitions") {
      config.repetitions = parse_integer<std::uint64_t>(option_value(arguments, index), option);
    } else if (option == "--format") {
      config.format = parse_format(option_value(arguments, index));
    } else if (option == "--output") {
      config.output_path = std::filesystem::path{std::string{option_value(arguments, index)}};
    } else {
      throw std::invalid_argument("未知 run 参数: " + std::string{option});
    }
  }
  return config;
}

auto CliApplication::run(std::span<const std::string_view> arguments) const -> int {
  try {
    if (arguments.empty()) {
      print_help();
      return EXIT_SUCCESS;
    }

    const auto command = arguments.front();
    const auto rest = arguments.subspan(1);
    if (command == "--help" || command == "-h" || command == "help") {
      print_help();
      return EXIT_SUCCESS;
    }
    if (command == "--version" || command == "-V") {
      std::cout << kProjectName << ' ' << kVersion << '\n';
      return EXIT_SUCCESS;
    }
    if (command == "run") {
      return run_command(rest);
    }
    if (command == "list") {
      print_workloads(benchmark::WorkloadRegistry::create_default());
      return EXIT_SUCCESS;
    }
    if (command == "system-info") {
      print_system_info();
      return EXIT_SUCCESS;
    }
    if (command == "engine-info") {
      print_engine_adapters();
      return EXIT_SUCCESS;
    }
    if (command == "profile" && rest.size() == 1 && rest.front() == "list") {
      print_frame_profiles();
      return EXIT_SUCCESS;
    }
    if (command == "compare") {
      if (rest.size() != 2) {
        throw std::invalid_argument("compare 需要两个 JSON 结果文件路径");
      }
      return compare_results(rest[0], rest[1]);
    }

    throw std::invalid_argument("未知命令: " + std::string{command});
  } catch (const std::exception& exception) {
    std::cerr << "错误: " << exception.what() << '\n';
    return EXIT_FAILURE;
  }
}

} // namespace gamebench::core
