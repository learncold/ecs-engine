#include "App/command_line_options.h"

#include <charconv>
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
        option != "--dummies" && option != "--dummy-change-percent" &&
        option != "--seed" && option != "--warmup" && option != "--steps" &&
        option != "--dt" && option != "--output") {
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
      } else if (value == "archetype") {
        options.implementation = SimulationImplementation::Archetype;
      } else {
        throw std::invalid_argument(
            "Invalid value for --implementation: expected oop, ecs or "
            "archetype");
      }
    } else if (option == "--agents") {
      options.simulation_config.boid_count =
          ParseNumber<std::size_t>(value, option);
    } else if (option == "--dummies") {
      options.simulation_config.dummy_count =
          ParseNumber<std::size_t>(value, option);
    } else if (option == "--dummy-change-percent") {
      options.simulation_config.dummy_change_percent =
          ParseNumber<std::uint32_t>(value, option);
    } else if (option == "--seed") {
      options.simulation_config.random_seed =
          ParseNumber<std::uint32_t>(value, option);
    } else if (option == "--warmup") {
      options.benchmark_config.warmup_steps =
          ParseNumber<std::size_t>(value, option);
    } else if (option == "--steps") {
      options.benchmark_config.measurement_steps =
          ParseNumber<std::size_t>(value, option);
    } else if (option == "--output") {
      if (value.empty())
        throw std::invalid_argument("Output path must not be empty");
      options.output_path = value;
    } else {
      options.benchmark_config.fixed_delta_seconds =
          ParseNumber<float>(value, option);
    }
  }
  if (has_settings && !options.benchmark_enabled) {
    throw std::invalid_argument("Benchmark settings require --benchmark");
  }
  // Validate before allocating or initializing the simulation.
  benchmarker::ValidateExperimentConfig(options.simulation_config,
                                        options.benchmark_config);
  return options;
}

void PrintUsage(std::ostream& output) {
  output
      << "Usage: App.exe [--benchmark [--implementation oop|ecs|archetype]\n"
         "               [--agents N] [--dummies N]\n"
         "               [--dummy-change-percent 0..100]\n"
         "               [--seed N] [--warmup N]\n"
         "               [--steps N] [--dt SECONDS] [--output FILE.csv]]\n"
         "       App.exe --help\n"
         "Without options: interactive 3D simulation.\n"
         "Benchmark defaults: implementation=ecs, agents=100 (Boids), "
         "dummies=0, dummy-change-percent=0, seed=42, warmup=100, steps=1000, "
         "dt=1/60 s.\n"
         "Boid/Dummy counts and warmup may be zero; steps and dt must be "
         "positive.\n"
         "Raw CSV is saved to --output or "
         "out/benchmarks/<implementation>-<run-id>.csv.\n";
}

}  // namespace app
