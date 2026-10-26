#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "Benchmarker/benchmarker.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/Systems/boid_orientation_system.h"
#include "Boids/ECS/Systems/boid_system.h"
#include "Boids/ECS/Systems/boundary_system.h"
#include "Boids/ECS/Systems/movement_system.h"
#include "Boids/ECS/boid_simulation.h"
#include "Boids/OOP/boid_simulation.h"

namespace {

class TestSimulation {
 public:
  explicit TestSimulation(const boids::BoidSimulationConfig&) {}
  void Initialize() { initialized_ = true; }
  void Update(float dt, engine::measurement::TimingRecorder& recorder) {
    EXPECT_TRUE(initialized_);
    EXPECT_FLOAT_EQ(dt, 0.025F);
    ++updates_;
    recorder.record_zero("entity_changes");
    recorder.record_zero("query_preparation");
    recorder.start("neighbor_calculation");
    recorder.stop("neighbor_calculation");
    recorder.record_zero("state_update");
  }
  engine::measurement::StorageReport GetStorageReport() const {
    EXPECT_EQ(updates_, 5U);  // Two warmup and three measured steps.
    engine::measurement::StorageReport report;
    report.add_bytes("test", 16U, 32U);
    return report;
  }

 private:
  bool initialized_{false};
  std::size_t updates_{0U};
};

TEST(BenchmarkerExperimentTest, OwnsFixedTimestepWarmupAndRawSamples) {
  const auto result =
      benchmarker::RunBenchmark<TestSimulation>({},
                                                {.fixed_delta_seconds = 0.025F,
                                                 .warmup_steps = 2U,
                                                 .measurement_steps = 3U},
                                                "test");
  EXPECT_EQ(result.timing.measured_steps, 3U);
  EXPECT_EQ(result.timing.sections[2].calls, 3U);
  EXPECT_GE(result.initialization_ms, 0.0);
  EXPECT_EQ(result.storage.size_bytes(), 16U);
  EXPECT_FALSE(result.environment.empty());
}

TEST(BenchmarkerExperimentTest,
     RejectsInvalidConditionsBeforeCreatingSimulation) {
  for (float dt : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::quiet_NaN()}) {
    EXPECT_THROW(benchmarker::ValidateConfig({.fixed_delta_seconds = dt}),
                 std::invalid_argument);
  }
  EXPECT_THROW(benchmarker::ValidateConfig({.measurement_steps = 0U}),
               std::invalid_argument);
}

TEST(BenchmarkerExperimentTest, RejectsDummySettingsInBoidOnlyRunner) {
  for (int condition = 0; condition < 3; ++condition) {
    boids::BoidSimulationConfig config;
    config.boid_count = 1U;
    config.dummy_count = condition == 1 ? 0U : 1U;
    config.dummy_change_percent = condition == 0 ? 0U : 100U;
    EXPECT_THROW(
        static_cast<void>(benchmarker::RunBenchmark<boids::oop::BoidSimulation>(
            config, {.warmup_steps = 0U, .measurement_steps = 1U}, "oop")),
        std::invalid_argument);
  }
}

TEST(BenchmarkerExperimentTest,
     ExportsSettingsRawSamplesMissingStatusAndCsvEscaping) {
  const auto result =
      benchmarker::RunBenchmark<TestSimulation>({},
                                                {.fixed_delta_seconds = 0.025F,
                                                 .warmup_steps = 2U,
                                                 .measurement_steps = 3U},
                                                "test");
  auto modified = result;
  modified.environment.emplace_back("quoted", "CPU, \"name\"\nline");
  modified.storage.add_unavailable(
      "opaque", engine::measurement::MeasurementStatus::Unsupported, "unknown");
  modified.timing.sections[0].samples[0].recorded = false;
  --modified.timing.sections[0].recorded_steps;
  modified.timing.sections[0].mean_ms_per_step.reset();
  std::ostringstream csv;
  benchmarker::WriteCsv(csv, modified);
  const auto text = csv.str();
  EXPECT_NE(text.find("kind,name,step,value,status,reason\n"),
            std::string::npos);
  EXPECT_NE(text.find("\"CPU, \"\"name\"\"\nline\""), std::string::npos);
  EXPECT_NE(text.find("\"step\",\"overall_ms\",\"2\""), std::string::npos);
  EXPECT_NE(
      text.find(
          "\"section_ms\",\"entity_changes\",\"0\",\"\",\"not_measured\""),
      std::string::npos);
  EXPECT_NE(
      text.find(
          "\"section_ms\",\"query_preparation\",\"0\",\"0\",\"available\""),
      std::string::npos);
  EXPECT_NE(
      text.find("\"storage_size_bytes\",\"opaque\",\"\",\"\",\"unsupported\""),
      std::string::npos);
  EXPECT_NE(text.find("\"build_flags\""), std::string::npos);
}

TEST(BenchmarkerExperimentTest, UndefinedDerivedMetricsRetainStatusAndReason) {
  boids::BoidSimulationConfig config;
  config.boid_count = 0U;
  const auto result =
      benchmarker::RunBenchmark<TestSimulation>(config,
                                                {.fixed_delta_seconds = 0.025F,
                                                 .warmup_steps = 2U,
                                                 .measurement_steps = 3U},
                                                "test");
  std::ostringstream csv;
  benchmarker::WriteCsv(csv, result);
  const auto text = csv.str();
  EXPECT_NE(text.find("\"neighbor_ms_per_pair\",\"\",\"\",\"not_measured\""),
            std::string::npos);
  EXPECT_NE(text.find("\"storage_size_bytes_per_entity\",\"\",\"\",\"not_"
                      "measured\",\"Entity count is zero\""),
            std::string::npos);
  EXPECT_NE(text.find("\"storage_capacity_bytes_per_entity\",\"\",\"\",\"not_"
                      "measured\",\"Entity count is zero\""),
            std::string::npos);
}

TEST(BenchmarkerExperimentTest,
     BothBoidsImplementationsProvideEverySectionAndStorage) {
  boids::BoidSimulationConfig simulation_config;
  simulation_config.boid_count = 8U;
  const benchmarker::BenchmarkConfig config{.warmup_steps = 0U,
                                            .measurement_steps = 3U};
  const auto ecs = benchmarker::RunBenchmark<boids::ecs::BoidSimulation>(
      simulation_config, config, "ecs");
  const auto oop = benchmarker::RunBenchmark<boids::oop::BoidSimulation>(
      simulation_config, config, "oop");
  for (const auto* result : {&ecs, &oop}) {
    ASSERT_EQ(result->timing.sections.size(), 4U);
    for (const auto& section : result->timing.sections) {
      EXPECT_EQ(section.recorded_steps, 3U) << section.name;
    }
    EXPECT_GT(result->storage.size_bytes(), 0U);
    EXPECT_GE(result->storage.capacity_bytes(), result->storage.size_bytes());
  }
  EXPECT_TRUE(oop.storage.complete());
  EXPECT_FALSE(ecs.storage.complete());  // Hash-table links/buckets are opaque.
  bool found_cache = false;
  for (const auto& entry : ecs.storage.entries()) {
    if (entry.name == "neighbors.cached_entries") {
      found_cache = true;
      ASSERT_TRUE(entry.size_bytes.has_value());
      EXPECT_GT(*entry.size_bytes, 0U);
    }
  }
  EXPECT_TRUE(found_cache);
  // All four systems are separate manager-owned allocations, including Boid.
  const std::size_t object_sizes[] = {
      sizeof(boids::ecs::BoidSystem), sizeof(boids::ecs::MovementSystem),
      sizeof(boids::ecs::BoundarySystem),
      sizeof(boids::ecs::BoidOrientationSystem)};
  for (std::size_t index = 0; index < 4U; ++index) {
    const std::string name =
        index == 0U ? "systems.0.object"
                    : "state_systems." + std::to_string(index - 1U) + ".object";
    bool found = false;
    for (const auto& entry : ecs.storage.entries()) {
      if (entry.name == name) {
        found = true;
        EXPECT_EQ(entry.size_bytes, object_sizes[index]);
      }
    }
    EXPECT_TRUE(found) << name;
  }
  const auto& state = ecs.timing.sections[3];
  EXPECT_EQ(state.calls, 3U);  // One continuous interval per measured step.
  for (const auto& sample : state.samples) EXPECT_EQ(sample.calls, 1U);
}

TEST(BenchmarkerExperimentTest, InstrumentationPreservesSimulationState) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  boids::ecs::BoidSimulation baseline(config);
  boids::ecs::BoidSimulation measured(config);
  boids::oop::BoidSimulation oop(config);
  baseline.Initialize();
  measured.Initialize();
  oop.Initialize();
  engine::measurement::TimingRecorder ecs_recorder(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  engine::measurement::TimingRecorder oop_recorder(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  ecs_recorder.begin_measurement(10U);
  oop_recorder.begin_measurement(10U);
  for (int step = 0; step < 10; ++step) {
    baseline.Update(0.02F);
    ecs_recorder.begin_step_measurement();
    measured.Update(0.02F, ecs_recorder);
    ecs_recorder.end_step_measurement();
    oop_recorder.begin_step_measurement();
    oop.Update(0.02F, oop_recorder);
    oop_recorder.end_step_measurement();
  }
  std::size_t index = 0U;
  measured.GetRegistry()
      .CreateView<engine::renderer::Transform, boids::ecs::Velocity>()
      .Each([&](engine::ecs::Entity entity,
                const engine::renderer::Transform& transform,
                const boids::ecs::Velocity& velocity) {
        const auto* expected =
            baseline.GetRegistry().FindComponent<engine::renderer::Transform>(
                entity);
        ASSERT_NE(expected, nullptr);
        for (int axis = 0; axis < 3; ++axis) {
          EXPECT_FLOAT_EQ(transform.position[axis], expected->position[axis]);
          EXPECT_NEAR(transform.position[axis],
                      oop.Objects()[index]->Position()[axis], 0.00001F);
          EXPECT_NEAR(velocity.value[axis],
                      oop.Objects()[index]->Velocity()[axis], 0.00001F);
        }
        ++index;
      });
  EXPECT_EQ(index, config.boid_count);
}

}  // namespace
