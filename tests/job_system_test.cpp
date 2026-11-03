#include "runtime/job_system.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <stdexcept>
#include <vector>

TEST(JobSystemTest, RespectsDependencies) {
  gamebench::runtime::JobSystem system({.worker_count = 2,
                                        .main_thread_participates = true,
                                        .scheduler = gamebench::core::SchedulerKind::WorkStealing,
                                        .cpu_affinity = {}});

  std::atomic<int> stage{0};
  auto first = system.submit([&stage] { stage.store(1); });
  const std::array dependencies{first};
  auto second = system.submit([&stage] { stage.fetch_add(1); }, dependencies);

  system.wait(second);
  EXPECT_EQ(stage.load(), 2);
}

TEST(JobSystemTest, ExecutesTasksWithAllSchedulerModes) {
  constexpr std::array modes{
      gamebench::core::SchedulerKind::GlobalQueue,
      gamebench::core::SchedulerKind::StaticPartitioning,
      gamebench::core::SchedulerKind::WorkStealing,
  };

  for (const auto mode : modes) {
    gamebench::runtime::JobSystem system({.worker_count = 3,
                                          .main_thread_participates = true,
                                          .scheduler = mode,
                                          .cpu_affinity = {}});
    std::atomic<unsigned> count{0};
    std::vector<gamebench::runtime::JobHandle> jobs;
    for (unsigned index = 0; index < 64; ++index) {
      jobs.push_back(system.submit([&count] { count.fetch_add(1); }));
    }
    system.wait_all(jobs);
    EXPECT_EQ(count.load(), 64U);
    const auto statistics = system.statistics();
    EXPECT_EQ(statistics.submitted_jobs, 64U);
    EXPECT_EQ(statistics.completed_jobs, 64U);
  }
}

TEST(JobSystemTest, PropagatesTaskExceptions) {
  gamebench::runtime::JobSystem system({.worker_count = 1,
                                        .main_thread_participates = false,
                                        .scheduler = gamebench::core::SchedulerKind::WorkStealing,
                                        .cpu_affinity = {}});

  auto job = system.submit([] { throw std::runtime_error("工作失败"); });
  EXPECT_THROW(system.wait(job), std::runtime_error);
}

TEST(JobSystemTest, ShutdownWaitsForSubmittedWork) {
  std::atomic<unsigned> count{0};
  {
    gamebench::runtime::JobSystem system({.worker_count = 2,
                                          .main_thread_participates = false,
                                          .scheduler = gamebench::core::SchedulerKind::GlobalQueue,
                                          .cpu_affinity = {}});
    for (unsigned index = 0; index < 32; ++index) {
      (void)system.submit([&count] { count.fetch_add(1); });
    }
    system.shutdown();
    EXPECT_EQ(count.load(), 32U);
  }
}
