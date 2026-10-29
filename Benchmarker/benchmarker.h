#pragma once

#include <cstddef>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Boids/Common/boid_simulation_config.h"
#include "Boids/Common/simulation_input.h"
#include "Engine/Measurement/memory_measurement.h"
#include "Engine/Measurement/timing_recorder.h"

namespace benchmarker {

struct BenchmarkConfig {
  float fixed_delta_seconds{1.0F / 60.0F};
  std::size_t warmup_steps{100U};
  std::size_t measurement_steps{1000U};
};

using Environment = std::vector<std::pair<std::string, std::string>>;
void ValidateConfig(const BenchmarkConfig& config);
void ValidateExperimentConfig(const boids::BoidSimulationConfig& simulation,
                              const BenchmarkConfig& benchmark);
[[nodiscard]] Environment CollectEnvironment();
[[nodiscard]] engine::measurement::StorageReport InputStorageReport(
    const boids::SimulationInput& input);

struct ExperimentResult {
  boids::BoidSimulationConfig simulation_config;
  BenchmarkConfig config;
  std::string implementation;
  Environment environment;
  double initialization_ms{0.0};
  engine::measurement::TimingResult timing;
  engine::measurement::ProcessMemoryResult process_memory;
  engine::measurement::StorageReport storage;
  engine::measurement::StorageReport input_storage;
};

// This experiment module owns orchestration. Engine only records intervals.
// Simulation must expose Initialize(input), Update(dt, dummy_ids, recorder) and
// GetStorageReport(). No callback or base-class interface is required.
template <typename Simulation>
[[nodiscard]] ExperimentResult RunBenchmark(
    const boids::BoidSimulationConfig& simulation_config,
    const BenchmarkConfig& config, std::string_view implementation) {
  ValidateExperimentConfig(simulation_config, config);
  Simulation simulation(simulation_config);
  const auto input = boids::PrepareSimulationInput(
      simulation_config, config.warmup_steps + config.measurement_steps);
  ExperimentResult result;
  result.simulation_config = simulation_config;
  result.config = config;
  result.implementation = implementation;
  result.environment = CollectEnvironment();

  engine::measurement::TimingRecorder initialization({"initialization"});
  engine::measurement::TimingRecorder recorder(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  // Input generation, constructor, environment and recorder setup are excluded.
  initialization.start("initialization");
  simulation.Initialize(input);
  initialization.stop("initialization");
  result.initialization_ms = initialization.total_ms("initialization");

  recorder.set_enabled(false);
  for (std::size_t step = 0; step < config.warmup_steps; ++step) {
    simulation.Update(config.fixed_delta_seconds, input.dummy_steps[step],
                      recorder);
  }
  recorder.begin_measurement(config.measurement_steps);
  for (std::size_t step = 0; step < config.measurement_steps; ++step) {
    recorder.begin_step_measurement();
    simulation.Update(config.fixed_delta_seconds,
                      input.dummy_steps[config.warmup_steps + step], recorder);
    recorder.end_step_measurement();
  }
  // Query process peaks before allocations for summaries/storage reports.
  result.process_memory = engine::measurement::QueryProcessMemory();
  result.storage = simulation.GetStorageReport();
  result.input_storage = InputStorageReport(input);
  result.timing = recorder.result();
  return result;
}

void PrintSummary(std::ostream& output, const ExperimentResult& result);
// Long-form CSV preserves every step/section and its status, plus settings,
// environment, summaries and itemized storage estimates. No output is timed.
void WriteCsv(std::ostream& output, const ExperimentResult& result);

}  // namespace benchmarker
