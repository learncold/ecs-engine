#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

#include "Boids/Common/boid_simulation_config.h"
#include "Engine/Benchmark/benchmarker.h"

namespace app {

enum class SimulationImplementation {
  Ecs,
  Oop,
};

struct CommandLineOptions {
  bool benchmark_enabled{false};
  bool help{false};
  SimulationImplementation implementation{SimulationImplementation::Ecs};
  boids::BoidSimulationConfig simulation_config;
  engine::benchmark::BenchmarkConfig benchmark_config;
};

[[nodiscard]] CommandLineOptions ParseCommandLineOptions(
    std::span<const std::string_view> arguments);
void PrintUsage(std::ostream& output);
void PrintBenchmarkResult(std::ostream& output,
                          const CommandLineOptions& options,
                          const engine::benchmark::BenchmarkResult& result);

template <typename Simulation>
engine::benchmark::BenchmarkResult RunBenchmark(
    const app::CommandLineOptions& options) {
  Simulation simulation(options.simulation_config);
  simulation.Initialize();

  const engine::benchmark::Benchmarker benchmarker(options.benchmark_config);

  return benchmarker.Benchmark(
      [&simulation](float dt) { simulation.Update(dt); });
}

}  // namespace app
