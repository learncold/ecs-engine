#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

#include "Boids/Common/boid_simulation_config.h"
#include "Engine/Benchmark/benchmarker.h"

namespace app {

struct CommandLineOptions {
  bool benchmark_enabled{false};
  bool help{false};
  boids::BoidSimulationConfig simulation_config;
  engine::benchmark::BenchmarkConfig benchmark_config;
};

[[nodiscard]] CommandLineOptions ParseCommandLineOptions(
    std::span<const std::string_view> arguments);
void PrintUsage(std::ostream& output);
void PrintBenchmarkResult(std::ostream& output,
                          const CommandLineOptions& options,
                          const engine::benchmark::BenchmarkResult& result);

}  // namespace app
