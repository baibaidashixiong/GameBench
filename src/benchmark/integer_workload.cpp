#include "benchmark/integer_workload.hpp"

#include <cstdint>
#include <string>

namespace gamebench::benchmark {
namespace {

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

} // namespace

auto IntegerWorkload::id() const -> std::string_view {
  return "integer";
}

auto IntegerWorkload::version() const -> std::string_view {
  return "1";
}

auto IntegerWorkload::category() const -> std::string_view {
  return "synthetic";
}

auto IntegerWorkload::description() const -> std::string_view {
  return "用于验证 Runner 的确定性整数混合负载";
}

auto IntegerWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  std::uint64_t accumulator = config.seed;
  for (std::uint64_t index = 0; index < config.objects; ++index) {
    accumulator += mix64(config.seed ^ index);
    accumulator ^= accumulator >> 17U;
  }

  WorkloadOutput output;
  output.checksum = accumulator;
  output.operations = config.objects;
  output.valid = accumulator != 0 && output.operations == config.objects;
  if (!output.valid) {
    output.validation_message = "整数工作负载校验失败";
  }
  return output;
}

} // namespace gamebench::benchmark
