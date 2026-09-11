#pragma once

#include <cstddef>
#include <functional>

namespace engine::benchmark {

struct BenchmarkConfig {
  float fixed_delta_seconds{1.0F / 60.0F};
  std::size_t warmup_steps{100U};
  std::size_t measurement_steps{1000U};
};

struct BenchmarkResult {
  std::size_t measured_steps{0U};
  double total_update_ms{0.0};
  double mean_update_ms{0.0};
  double updates_per_second{0.0};
};

class Benchmarker {
 public:
  using UpdateCallback = std::function<void(float)>;

  explicit Benchmarker(BenchmarkConfig config = {});

  // Synchronously invokes the callback. Initialization and reporting belong
  // to the caller; warmup calls are excluded from the measured interval.
  [[nodiscard]] BenchmarkResult Benchmark(const UpdateCallback& update) const;

 private:
  BenchmarkConfig config_;
};

}  // namespace engine::benchmark
