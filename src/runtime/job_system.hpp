#pragma once

#include "core/config.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace gamebench::runtime {

namespace detail {
struct JobState;
}

struct JobSystemConfig {
  unsigned worker_count{};
  bool main_thread_participates{true};
  core::SchedulerKind scheduler{core::SchedulerKind::WorkStealing};
  std::vector<unsigned> cpu_affinity;
};

struct SchedulerStatistics {
  std::uint64_t submitted_jobs{};
  std::uint64_t completed_jobs{};
  std::uint64_t stolen_jobs{};
  std::uint64_t main_thread_jobs{};
  std::uint64_t exceptions{};
  unsigned affinity_bound_workers{};
  std::vector<double> worker_busy_ms;
};

class JobHandle final {
public:
  JobHandle() = default;

  [[nodiscard]] auto valid() const -> bool;
  [[nodiscard]] auto done() const -> bool;
  void wait() const;
  void rethrow_if_failed() const;

private:
  explicit JobHandle(std::shared_ptr<detail::JobState> state);

  std::shared_ptr<detail::JobState> state_;

  friend class JobSystem;
};

class JobSystem final {
public:
  using JobFunction = std::function<void()>;

  explicit JobSystem(JobSystemConfig config);
  ~JobSystem();

  JobSystem(const JobSystem&) = delete;
  auto operator=(const JobSystem&) -> JobSystem& = delete;
  JobSystem(JobSystem&&) = delete;
  auto operator=(JobSystem&&) -> JobSystem& = delete;

  [[nodiscard]] auto submit(JobFunction job, std::span<const JobHandle> dependencies = {})
      -> JobHandle;
  void wait(const JobHandle& handle);
  void wait_all(std::span<const JobHandle> handles);
  void shutdown();

  [[nodiscard]] auto statistics() const -> SchedulerStatistics;
  [[nodiscard]] auto worker_count() const -> unsigned;
  [[nodiscard]] auto scheduler() const -> core::SchedulerKind;

private:
  struct WorkerQueue;
  struct StatisticsState;

  static void execute_job(const std::shared_ptr<detail::JobState>& job);
  void notify_dependents(const std::shared_ptr<detail::JobState>& job);
  void enqueue(const std::shared_ptr<detail::JobState>& job);
  [[nodiscard]] auto try_run_one() -> bool;
  [[nodiscard]] auto pop_for_worker(unsigned worker_index, bool& stolen)
      -> std::shared_ptr<detail::JobState>;
  void worker_loop(unsigned worker_index);
  void complete_job(const std::shared_ptr<detail::JobState>& job, double elapsed_ms,
                    bool executed_by_main, bool stolen, int worker_index);
  void bind_worker_affinity(unsigned worker_index) noexcept;

  JobSystemConfig config_;
  std::vector<std::unique_ptr<WorkerQueue>> local_queues_;
  std::unique_ptr<StatisticsState> statistics_;
  std::vector<std::jthread> workers_;
  std::atomic<std::uint64_t> pending_jobs_{0};
  std::atomic<std::size_t> next_worker_{0};
  std::atomic<bool> accepting_jobs_{true};
  std::atomic<bool> stopping_{false};
  std::atomic<unsigned> affinity_bound_workers_{0};
  std::mutex tracked_jobs_mutex_;
  std::unordered_set<std::shared_ptr<detail::JobState>> tracked_jobs_;
};

} // namespace gamebench::runtime
