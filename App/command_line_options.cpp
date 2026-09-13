#include "App/command_line_options.h"

#include <charconv>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>

namespace app {
namespace {

template <typename T>
T ParseNumber(std::string_view value, std::string_view option) {
  T number{};
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), number);
  if (error != std::errc{} || end != value.data() + value.size()) {
    throw std::invalid_argument("Invalid value for " + std::string(option));
  }
  return number;
}

}  // namespace

CommandLineOptions ParseCommandLineOptions(
    std::span<const std::string_view> arguments) {
  CommandLineOptions options;
  bool has_settings = false;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const auto option = arguments[index];
    if (option == "--benchmark") {
      options.benchmark_enabled = true;
      continue;
    }
    if (option == "--help") {
      options.help = true;
      continue;
    }
    if (option != "--implementation" && option != "--agents" &&
        option != "--seed" && option != "--warmup" && option != "--steps" &&
        option != "--dt") {
      throw std::invalid_argument("Unknown option: " + std::string(option));
    }
    if (++index == arguments.size()) {
      throw std::invalid_argument("Missing value for " + std::string(option));
    }
    const auto value = arguments[index];
    has_settings = true;
    if (option == "--implementation") {
      if (value == "ecs") {
        options.implementation = SimulationImplementation::Ecs;
      } else if (value == "oop") {
        options.implementation = SimulationImplementation::Oop;
      } else {
        throw std::invalid_argument(
            "Invalid value for --implementation: expected ecs or oop");
      }
    } else if (option == "--agents") {
      options.simulation_config.boid_count =
          ParseNumber<std::size_t>(value, option);
    } else if (option == "--seed") {
      options.simulation_config.random_seed =
          ParseNumber<std::uint32_t>(value, option);
    } else if (option == "--warmup") {
      options.benchmark_config.warmup_steps =
          ParseNumber<std::size_t>(value, option);
    } else if (option == "--steps") {
      options.benchmark_config.measurement_steps =
          ParseNumber<std::size_t>(value, option);
    } else {
      options.benchmark_config.fixed_delta_seconds =
          ParseNumber<float>(value, option);
    }
  }
  if (has_settings && !options.benchmark_enabled) {
    throw std::invalid_argument("Benchmark settings require --benchmark");
  }
  if (options.simulation_config.boid_count == 0U) {
    throw std::invalid_argument("Agent count must be positive");
  }
  // Validate before allocating or initializing the simulation.
  const engine::benchmark::Benchmarker validation(options.benchmark_config);
  return options;
}

void PrintUsage(std::ostream& output) {
  output << "Usage: App.exe [--benchmark [--implementation ecs|oop]\n"
            "               [--agents N] [--seed N] [--warmup N]\n"
            "               [--steps N] [--dt SECONDS]]\n"
            "       App.exe --help\n"
            "Without options: interactive 3D simulation.\n"
            "Benchmark defaults: agents=100, seed=42, warmup=100, steps=1000, "
            "dt=1/60 s.\n";
}

void PrintBenchmarkResult(std::ostream& output,
                          const CommandLineOptions& options,
                          const engine::benchmark::BenchmarkResult& result) {
  const auto& simulation_config = options.simulation_config;
  const auto& parameters = simulation_config.parameters;
  const char* implementation =
      options.implementation == SimulationImplementation::Ecs ? "ECS"
                                                              : "OOP";
  output << std::setprecision(9) << "Benchmark: " << implementation
         << " / naive / simulation-only\n"
#ifdef NDEBUG
         << "Build: Release (NDEBUG)\n"
#else
         << "Build: Debug (use Release for performance comparisons)\n"
#endif
         << "Agents: " << simulation_config.boid_count
         << "\nSeed: " << simulation_config.random_seed
         << "\nFixed timestep (s): "
         << options.benchmark_config.fixed_delta_seconds
         << "\nWarmup steps: " << options.benchmark_config.warmup_steps
         << "\nMeasured steps: " << result.measured_steps
         << "\nSpawn half extent: " << simulation_config.spawn_half_extent
         << "\nBoundary half extent: " << simulation_config.boundary_half_extent
         << "\nInitial speed: " << simulation_config.initial_speed
         << "\nNeighbor radius: " << parameters.neighbor_radius
         << "\nSeparation radius: " << parameters.separation_radius
         << "\nSeparation weight: " << parameters.separation_weight
         << "\nAlignment weight: " << parameters.alignment_weight
         << "\nCohesion weight: " << parameters.cohesion_weight
         << "\nPreferred speed: " << parameters.preferred_speed
         << "\nMax speed: " << parameters.max_speed
         << "\nMax alignment force: " << parameters.max_alignment_force
         << "\nTotal update time (ms): " << result.total_update_ms
         << "\nMean update time (ms/step): " << result.mean_update_ms
         << "\nUpdates/s: " << result.updates_per_second << '\n';
}

}  // namespace app
