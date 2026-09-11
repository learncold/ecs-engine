#include "Engine/Benchmark/benchmarker.h"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace engine::benchmark {

Benchmarker::Benchmarker(BenchmarkConfig config) : config_(config) {
  if (!std::isfinite(config_.fixed_delta_seconds) ||
      config_.fixed_delta_seconds <= 0.0F) {
    throw std::invalid_argument(
        "Benchmark timestep must be finite and positive");
  }
  if (config_.measurement_steps == 0U) {
    throw std::invalid_argument("Benchmark measured steps must be positive");
  }
}

BenchmarkResult Benchmarker::Benchmark(const UpdateCallback& update) const {
  if (!update) {
    throw std::invalid_argument("Benchmark update callback must not be empty");
  }

  for (std::size_t step = 0; step < config_.warmup_steps; ++step) {
    update(config_.fixed_delta_seconds);
  }

  using Clock = std::chrono::steady_clock;
  BenchmarkResult result{.measured_steps = config_.measurement_steps};
  for (std::size_t step = 0; step < config_.measurement_steps; ++step) {
    const auto begin = Clock::now();
    update(config_.fixed_delta_seconds);
    const auto end = Clock::now();
    result.total_update_ms +=
        std::chrono::duration<double, std::milli>(end - begin).count();
  }

  result.mean_update_ms =
      result.total_update_ms / static_cast<double>(result.measured_steps);
  if (result.total_update_ms > 0.0) {
    result.updates_per_second = static_cast<double>(result.measured_steps) *
                                1000.0 / result.total_update_ms;
  }
  return result;
}

}  // namespace engine::benchmark
