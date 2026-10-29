#pragma once

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

#include "Benchmarker/benchmarker.h"
#include "Boids/Common/boid_simulation_config.h"

namespace app {

enum class SimulationImplementation {
  Ecs,
  Oop,
  Archetype,
};

struct CommandLineOptions {
  bool benchmark_enabled{false};
  bool help{false};
  SimulationImplementation implementation{SimulationImplementation::Ecs};
  boids::BoidSimulationConfig simulation_config;
  benchmarker::BenchmarkConfig benchmark_config;
  std::string output_path;
};

[[nodiscard]] CommandLineOptions ParseCommandLineOptions(
    std::span<const std::string_view> arguments);
void PrintUsage(std::ostream& output);

}  // namespace app
