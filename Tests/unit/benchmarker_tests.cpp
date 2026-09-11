#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>

#include "Engine/Benchmark/benchmarker.h"

namespace {

TEST(BenchmarkerTest, UsesFixedTimestepAndReportsOnlyMeasuredSteps) {
  const engine::benchmark::Benchmarker benchmarker(
      {.fixed_delta_seconds = 0.025F,
       .warmup_steps = 3,
       .measurement_steps = 7});
  int calls = 0;
  const auto result = benchmarker.Benchmark([&](float dt) {
    EXPECT_FLOAT_EQ(dt, 0.025F);
    ++calls;
  });
  EXPECT_EQ(calls, 10);
  EXPECT_EQ(result.measured_steps, 7U);
  EXPECT_GE(result.total_update_ms, 0.0);
  EXPECT_DOUBLE_EQ(result.mean_update_ms, result.total_update_ms / 7.0);
}

TEST(BenchmarkerTest, ExcludesWarmupTime) {
  const engine::benchmark::Benchmarker benchmarker(
      {.warmup_steps = 1, .measurement_steps = 2});
  int calls = 0;
  const auto begin = std::chrono::steady_clock::now();
  const auto result = benchmarker.Benchmark([&](float) {
    if (calls++ == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
  });
  const double elapsed_ms = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - begin)
                                .count();
  EXPECT_EQ(calls, 3);
  EXPECT_GE(elapsed_ms - result.total_update_ms, 20.0);
}

TEST(BenchmarkerTest, SupportsZeroWarmupAndIndependentRuns) {
  const engine::benchmark::Benchmarker benchmarker(
      {.warmup_steps = 0, .measurement_steps = 2});
  for (int run = 0; run < 2; ++run) {
    int calls = 0;
    const auto result = benchmarker.Benchmark([&](float) { ++calls; });
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(result.measured_steps, 2U);
  }
}

TEST(BenchmarkerTest, RejectsInvalidConfigAndEmptyCallback) {
  for (float dt : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::quiet_NaN()}) {
    EXPECT_THROW((engine::benchmark::Benchmarker{{.fixed_delta_seconds = dt}}),
                 std::invalid_argument);
  }
  EXPECT_THROW((engine::benchmark::Benchmarker{{.measurement_steps = 0}}),
               std::invalid_argument);
  EXPECT_THROW(
      static_cast<void>(engine::benchmark::Benchmarker{}.Benchmark({})),
      std::invalid_argument);
}

}  // namespace
