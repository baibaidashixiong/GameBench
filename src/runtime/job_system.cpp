#include "runtime/job_system.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include <pthread.h>
#include <sched.h>

namespace gamebench::runtime {

struct detail::JobState {
  JobSystem::JobFunction work;
  std::atomic<std::int64_t> remaining_dependencies{0};
  std::atomic<bool> completed{false};
  std::exception_ptr exception;
  std::mutex mutex;
  std::condition_variable completion;
  std::vector<std::weak_ptr<JobState>> dependents;
};

struct JobSystem::WorkerQueue {
  std::mutex mutex;
  std::deque<std::shared_ptr<detail::JobState>> jobs;
};

struct JobSystem::StatisticsState {
  mutable std::mutex mutex;
  std::uint64_t submitted{};
  std::uint64_t completed{};
  std::uint64_t stolen{};
  std::uint64_t main_thread{};
  std::uint64_t exceptions{};
  std::vector<double> worker_busy_ms;
};

namespace {

[[nodiscard]] auto now() -> std::chrono::steady_clock::time_point {
  return std::chrono::steady_clock::now();
}

[[nodiscard]] auto elapsed_ms(std::chrono::steady_clock::time_point start) -> double {
  return std::chrono::duration<double, std::milli>(now() - start).count();
}

} // namespace

JobHandle::JobHandle(std::shared_ptr<detail::JobState> state) : state_(std::move(state)) {}

auto JobHandle::valid() const -> bool {
  return static_cast<bool>(state_);
}

auto JobHandle::done() const -> bool {
  return !state_ || state_->completed.load();
}

void JobHandle::wait() const {
  if (!state_) {
    return;
  }
  std::unique_lock lock(state_->mutex);
  state_->completion.wait(lock, [this] { return state_->completed.load(); });
}

void JobHandle::rethrow_if_failed() const {
  if (!state_) {
    return;
  }
  wait();
  std::scoped_lock lock(state_->mutex);
  if (state_->exception) {
    std::rethrow_exception(state_->exception);
  }
}

JobSystem::JobSystem(JobSystemConfig config)
    : config_(std::move(config)), statistics_(std::make_unique<StatisticsState>()) {
  if (config_.worker_count == 0) {
    throw std::invalid_argument("JobSystem 至少需要一个 Worker 或使用主线程后参与模式");
  }
  local_queues_.reserve(config_.worker_count);
  statistics_->worker_busy_ms.resize(config_.worker_count, 0.0);
  for (unsigned index = 0; index < config_.worker_count; ++index) {
    local_queues_.push_back(std::make_unique<WorkerQueue>());
  }
  workers_.reserve(config_.worker_count);
  for (unsigned index = 0; index < config_.worker_count; ++index) {
    workers_.emplace_back([this, index] { worker_loop(index); });
  }
}

JobSystem::~JobSystem() {
  shutdown();
}

auto JobSystem::submit(JobFunction job, std::span<const JobHandle> dependencies) -> JobHandle {
  if (!accepting_jobs_.load()) {
    throw std::logic_error("JobSystem 已停止接收任务");
  }
  if (!job) {
    throw std::invalid_argument("任务函数不能为空");
  }
  for (const auto& dependency : dependencies) {
    if (!dependency.valid()) {
      throw std::invalid_argument("任务依赖不能为空");
    }
  }

  auto state = std::make_shared<detail::JobState>();
  state->work = std::move(job);
  state->remaining_dependencies.store(static_cast<std::int64_t>(dependencies.size()));
  pending_jobs_.fetch_add(1, std::memory_order_relaxed);
  {
    std::scoped_lock tracked_lock(tracked_jobs_mutex_);
    tracked_jobs_.insert(state);
  }

  {
    std::scoped_lock statistics_lock(statistics_->mutex);
    ++statistics_->submitted;
  }

  for (const auto& dependency : dependencies) {
    std::scoped_lock dependency_lock(dependency.state_->mutex);
    if (dependency.state_->completed.load()) {
      state->remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel);
    } else {
      dependency.state_->dependents.push_back(state);
    }
  }

  if (state->remaining_dependencies.load(std::memory_order_acquire) == 0) {
    enqueue(state);
  }
  return JobHandle{state};
}

void JobSystem::wait(const JobHandle& handle) {
  if (!handle.valid()) {
    return;
  }
  while (!handle.done()) {
    if (!config_.main_thread_participates || !try_run_one()) {
      std::unique_lock lock(handle.state_->mutex);
      handle.state_->completion.wait_for(lock, std::chrono::milliseconds{1},
                                         [&handle] { return handle.done(); });
    }
  }
  handle.rethrow_if_failed();
}

void JobSystem::wait_all(std::span<const JobHandle> handles) {
  for (const auto& handle : handles) {
    wait(handle);
  }
}

void JobSystem::shutdown() {
  if (!accepting_jobs_.exchange(false)) {
    return;
  }
  while (pending_jobs_.load(std::memory_order_acquire) != 0) {
    if (!config_.main_thread_participates || !try_run_one()) {
      std::this_thread::yield();
    }
  }
  stopping_.store(true, std::memory_order_release);
  workers_.clear();
}

auto JobSystem::statistics() const -> SchedulerStatistics {
  std::scoped_lock lock(statistics_->mutex);
  SchedulerStatistics snapshot;
  snapshot.submitted_jobs = statistics_->submitted;
  snapshot.completed_jobs = statistics_->completed;
  snapshot.stolen_jobs = statistics_->stolen;
  snapshot.main_thread_jobs = statistics_->main_thread;
  snapshot.exceptions = statistics_->exceptions;
  snapshot.affinity_bound_workers = affinity_bound_workers_.load();
  snapshot.worker_busy_ms = statistics_->worker_busy_ms;
  return snapshot;
}

auto JobSystem::worker_count() const -> unsigned {
  return config_.worker_count;
}

auto JobSystem::scheduler() const -> core::SchedulerKind {
  return config_.scheduler;
}

void JobSystem::execute_job(const std::shared_ptr<detail::JobState>& job) {
  try {
    JobFunction work;
    {
      std::scoped_lock lock(job->mutex);
      work = std::move(job->work);
    }
    work();
  } catch (...) {
    std::scoped_lock lock(job->mutex);
    job->exception = std::current_exception();
  }
}

void JobSystem::notify_dependents(const std::shared_ptr<detail::JobState>& job) {
  std::vector<std::weak_ptr<detail::JobState>> dependents;
  {
    std::scoped_lock lock(job->mutex);
    job->completed.store(true, std::memory_order_release);
    dependents = std::move(job->dependents);
  }
  job->completion.notify_all();

  for (const auto& weak_dependent : dependents) {
    if (auto dependent = weak_dependent.lock()) {
      if (dependent->remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        enqueue(dependent);
      }
    }
  }
}

void JobSystem::enqueue(const std::shared_ptr<detail::JobState>& job) {
  if (config_.scheduler == core::SchedulerKind::GlobalQueue) {
    auto& queue = *local_queues_.front();
    {
      std::scoped_lock lock(queue.mutex);
      queue.jobs.push_back(job);
    }
  } else {
    const auto index = next_worker_.fetch_add(1, std::memory_order_relaxed) % local_queues_.size();
    auto& queue = *local_queues_[index];
    std::scoped_lock lock(queue.mutex);
    queue.jobs.push_back(job);
  }
}

auto JobSystem::try_run_one() -> bool {
  bool stolen = false;
  const auto worker_index =
      next_worker_.fetch_add(1, std::memory_order_relaxed) % local_queues_.size();
  auto job = pop_for_worker(static_cast<unsigned>(worker_index), stolen);
  if (!job) {
    return false;
  }
  const auto start = now();
  execute_job(job);
  complete_job(job, elapsed_ms(start), true, stolen, -1);
  return true;
}

auto JobSystem::pop_for_worker(unsigned worker_index, bool& stolen)
    -> std::shared_ptr<detail::JobState> {
  stolen = false;
  if (config_.scheduler == core::SchedulerKind::GlobalQueue) {
    auto& queue = *local_queues_.front();
    std::scoped_lock lock(queue.mutex);
    if (queue.jobs.empty()) {
      return nullptr;
    }
    auto job = std::move(queue.jobs.front());
    queue.jobs.pop_front();
    return job;
  }

  {
    auto& queue = *local_queues_[worker_index];
    std::scoped_lock lock(queue.mutex);
    if (!queue.jobs.empty()) {
      auto job = std::move(queue.jobs.back());
      queue.jobs.pop_back();
      return job;
    }
  }

  if (config_.scheduler == core::SchedulerKind::StaticPartitioning) {
    return nullptr;
  }

  for (std::size_t offset = 1; offset < local_queues_.size(); ++offset) {
    const auto victim_index = (worker_index + offset) % local_queues_.size();
    auto& queue = *local_queues_[victim_index];
    std::scoped_lock lock(queue.mutex);
    if (!queue.jobs.empty()) {
      auto job = std::move(queue.jobs.front());
      queue.jobs.pop_front();
      stolen = true;
      return job;
    }
  }
  return nullptr;
}

void JobSystem::worker_loop(unsigned worker_index) {
  bind_worker_affinity(worker_index);
  std::string thread_name = "gb-worker-" + std::to_string(worker_index);
  pthread_setname_np(pthread_self(), thread_name.c_str());

  while (!stopping_.load(std::memory_order_acquire)) {
    bool stolen = false;
    auto job = pop_for_worker(worker_index, stolen);
    if (!job) {
      std::this_thread::yield();
      continue;
    }
    const auto start = now();
    execute_job(job);
    complete_job(job, elapsed_ms(start), false, stolen, static_cast<int>(worker_index));
  }
}

void JobSystem::complete_job(const std::shared_ptr<detail::JobState>& job, double elapsed_ms,
                             bool executed_by_main, bool stolen, int worker_index) {
  {
    std::scoped_lock tracked_lock(tracked_jobs_mutex_);
    tracked_jobs_.erase(job);
  }
  {
    std::scoped_lock lock(statistics_->mutex);
    ++statistics_->completed;
    if (executed_by_main) {
      ++statistics_->main_thread;
    } else {
      statistics_->worker_busy_ms[static_cast<std::size_t>(worker_index)] += elapsed_ms;
    }
    if (stolen) {
      ++statistics_->stolen;
    }
    if (job->exception) {
      ++statistics_->exceptions;
    }
  }
  notify_dependents(job);
  pending_jobs_.fetch_sub(1, std::memory_order_acq_rel);
}

void JobSystem::bind_worker_affinity(unsigned worker_index) noexcept {
  if (worker_index >= config_.cpu_affinity.size()) {
    return;
  }
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(config_.cpu_affinity[worker_index], &set);
  if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0) {
    affinity_bound_workers_.fetch_add(1, std::memory_order_relaxed);
  }
}

} // namespace gamebench::runtime
