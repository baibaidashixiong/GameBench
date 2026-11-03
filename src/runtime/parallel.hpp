#pragma once

#include "runtime/job_system.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace gamebench::runtime {

template<typename Function>
void parallel_for(JobSystem& system, std::size_t count, std::size_t grain_size,
                  Function&& function) {
  if (count == 0) {
    return;
  }
  grain_size = std::max<std::size_t>(1, grain_size);
  std::vector<JobHandle> jobs;
  const auto chunk_count = (count + grain_size - 1) / grain_size;
  jobs.reserve(chunk_count);
  for (std::size_t chunk = 0; chunk < chunk_count; ++chunk) {
    const auto begin = chunk * grain_size;
    const auto end = std::min(count, begin + grain_size);
    jobs.push_back(system.submit([begin, end, &function] { function(begin, end); }));
  }
  system.wait_all(jobs);
}

} // namespace gamebench::runtime
