#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "App/benchmark_output.h"
#include "Benchmarker/benchmarker.h"
#include "Boids/Archetype/boid_simulation.h"
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
  explicit TestSimulation(const boids::BoidSimulationConfig& config)
      : expected_(boids::PrepareSimulationInput(config, 5U)) {}
  void Initialize(const boids::SimulationInput& input) {
    initialized_ = true;
    input_ = &input;
    ASSERT_EQ(input.dummy_steps, expected_.dummy_steps);
    EXPECT_EQ(input.initial_objects.size(), expected_.initial_objects.size());
  }
  void Update(float dt, std::span<const std::size_t> ids,
              engine::measurement::TimingRecorder& recorder) {
    EXPECT_TRUE(initialized_);
    EXPECT_FLOAT_EQ(dt, 0.025F);
    ASSERT_LT(updates_, expected_.dummy_steps.size());
    EXPECT_EQ(ids.data(), input_->dummy_steps[updates_].data());
    EXPECT_EQ(std::vector<std::size_t>(ids.begin(), ids.end()),
              expected_.dummy_steps[updates_]);
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
  boids::SimulationInput expected_;
  const boids::SimulationInput* input_{nullptr};
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

TEST(BenchmarkerExperimentTest,
     ContinuesPreparedPlansThroughWarmupAndMeasurement) {
  boids::BoidSimulationConfig config;
  config.boid_count = 3U;
  config.dummy_count = 19U;
  config.dummy_change_percent = 40U;
  const auto result =
      benchmarker::RunBenchmark<TestSimulation>(config,
                                                {.fixed_delta_seconds = 0.025F,
                                                 .warmup_steps = 2U,
                                                 .measurement_steps = 3U},
                                                "test");
  const auto input = boids::PrepareSimulationInput(config, 5U);
  const auto expected = benchmarker::InputStorageReport(input);
  EXPECT_EQ(result.input_storage.size_bytes(), expected.size_bytes());
  EXPECT_EQ(result.input_storage.capacity_bytes(), expected.capacity_bytes());
  EXPECT_EQ(
      result.input_storage.size_bytes(),
      sizeof(input) +
          input.initial_objects.size() * sizeof(boids::InitialObjectState) +
          5U * sizeof(boids::DummyStepPlan) + 5U * 7U * sizeof(std::size_t));
  EXPECT_EQ(result.storage.size_bytes(), 16U);
}

TEST(BenchmarkerExperimentTest, InputStorageCountsSizeAndSpareCapacityOnce) {
  boids::SimulationInput input;
  input.initial_objects.reserve(7U);
  input.initial_objects.resize(1U);
  input.dummy_steps.reserve(5U);
  input.dummy_steps.resize(2U);
  input.dummy_steps[0].reserve(8U);
  input.dummy_steps[0].resize(3U);
  input.dummy_steps[1].reserve(4U);
  const auto report = benchmarker::InputStorageReport(input);
  EXPECT_TRUE(report.complete());
  EXPECT_EQ(report.size_bytes(),
            sizeof(input) + sizeof(boids::InitialObjectState) +
                2U * sizeof(boids::DummyStepPlan) + 3U * sizeof(std::size_t));
  EXPECT_EQ(
      report.capacity_bytes(),
      sizeof(input) +
          input.initial_objects.capacity() * sizeof(boids::InitialObjectState) +
          input.dummy_steps.capacity() * sizeof(boids::DummyStepPlan) +
          (input.dummy_steps[0].capacity() + input.dummy_steps[1].capacity()) *
              sizeof(std::size_t));
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

std::string CsvValue(const std::string& csv, std::string_view kind,
                     std::string_view name) {
  const std::string prefix =
      "\"" + std::string(kind) + "\",\"" + std::string(name) + "\",\"\",\"";
  const auto begin = csv.find(prefix);
  if (begin == std::string::npos) {
    ADD_FAILURE() << "Missing CSV row: " << kind << '/' << name;
    return {};
  }
  const auto value_begin = begin + prefix.size();
  return csv.substr(value_begin, csv.find('"', value_begin) - value_begin);
}

TEST(BenchmarkerExperimentTest,
     ThreeImplementationsExportActualConditionsAndDenominators) {
  for (const auto counts :
       {std::pair{0U, 0U}, std::pair{0U, 5U}, std::pair{1U, 0U},
        std::pair{3U, 2U}, std::pair{8U, 5U}}) {
    for (const auto percent : {0U, 40U, 100U}) {
      boids::BoidSimulationConfig config;
      config.boid_count = counts.first;
      config.dummy_count = counts.second;
      config.dummy_change_percent = percent;
      config.random_seed = 123U;
      config.parameters.alignment_weight = 0.5F;
      const benchmarker::BenchmarkConfig benchmark{.fixed_delta_seconds = 0.02F,
                                                   .warmup_steps = 2U,
                                                   .measurement_steps = 3U};
      const auto oop = benchmarker::RunBenchmark<boids::oop::BoidSimulation>(
          config, benchmark, "oop");
      const auto ecs = benchmarker::RunBenchmark<boids::ecs::BoidSimulation>(
          config, benchmark, "ecs");
      const auto archetype =
          benchmarker::RunBenchmark<boids::archetype::BoidSimulation>(
              config, benchmark, "archetype");
      for (const auto* result : {&oop, &ecs, &archetype}) {
        SCOPED_TRACE(result->implementation +
                     " Boids=" + std::to_string(counts.first) +
                     " Dummies=" + std::to_string(counts.second) +
                     " percent=" + std::to_string(percent));
        ASSERT_EQ(result->timing.sections.size(), 4U);
        for (const auto& section : result->timing.sections) {
          EXPECT_EQ(section.recorded_steps, 3U);
        }
        std::ostringstream stream;
        benchmarker::WriteCsv(stream, *result);
        const auto csv = stream.str();
        EXPECT_EQ(CsvValue(csv, "setting", "implementation"),
                  result->implementation);
        EXPECT_EQ(CsvValue(csv, "setting", "boid_count"),
                  std::to_string(counts.first));
        EXPECT_EQ(CsvValue(csv, "setting", "dummy_count"),
                  std::to_string(counts.second));
        EXPECT_EQ(CsvValue(csv, "setting", "dummy_change_percent"),
                  std::to_string(percent));
        EXPECT_EQ(CsvValue(csv, "setting", "dummy_change_count"),
                  std::to_string(counts.second * percent / 100U));
        EXPECT_EQ(CsvValue(csv, "setting", "seed"), "123");
        EXPECT_EQ(CsvValue(csv, "setting", "warmup_steps"), "2");
        EXPECT_EQ(CsvValue(csv, "setting", "measurement_steps"), "3");
        EXPECT_EQ(CsvValue(csv, "setting", "alignment_weight"), "0.5");
        EXPECT_DOUBLE_EQ(
            std::stod(CsvValue(csv, "setting", "fixed_delta_seconds")),
            static_cast<double>(benchmark.fixed_delta_seconds));
        EXPECT_EQ(CsvValue(csv, "summary", "input_storage_size_bytes"),
                  std::to_string(result->input_storage.size_bytes()));
        EXPECT_EQ(result->input_storage.size_bytes(),
                  oop.input_storage.size_bytes());
        const auto count = counts.first + counts.second;
        if (count > 0U) {
          EXPECT_DOUBLE_EQ(
              std::stod(
                  CsvValue(csv, "summary", "storage_size_bytes_per_entity")),
              static_cast<double>(result->storage.size_bytes()) / count);
          if (result->process_memory.peak_private_commit_bytes) {
            EXPECT_DOUBLE_EQ(
                std::stod(CsvValue(csv, "process_bytes_per_entity",
                                   "peak_private_commit")),
                static_cast<double>(
                    *result->process_memory.peak_private_commit_bytes) /
                    count);
          }
        } else {
          EXPECT_TRUE(CsvValue(csv, "summary", "storage_size_bytes_per_entity")
                          .empty());
        }
        const auto& neighbors = result->timing.sections[2];
        if (counts.first > 1U) {
          EXPECT_DOUBLE_EQ(
              std::stod(CsvValue(csv, "summary", "neighbor_ms_per_pair")),
              *neighbors.mean_ms_per_step /
                  (counts.first * (counts.first - 1U)));
        } else {
          EXPECT_TRUE(CsvValue(csv, "summary", "neighbor_ms_per_pair").empty());
        }
      }
    }
  }
}

TEST(BenchmarkerExperimentTest,
     PreservesUnavailableProcessMemoryAndActualZero) {
  auto result =
      benchmarker::RunBenchmark<TestSimulation>({},
                                                {.fixed_delta_seconds = 0.025F,
                                                 .warmup_steps = 2U,
                                                 .measurement_steps = 3U},
                                                "test");
  result.process_memory = {
      .status = engine::measurement::MeasurementStatus::Unsupported,
      .reason = "test platform"};
  std::ostringstream missing;
  benchmarker::WriteCsv(missing, result);
  EXPECT_NE(
      missing.str().find("\"process_bytes_per_entity\",\"peak_private_commit\","
                         "\"\",\"\",\"unsupported\",\"test platform\""),
      std::string::npos);
  result.process_memory = {
      .status = engine::measurement::MeasurementStatus::Available,
      .reason = {},
      .peak_private_commit_bytes = 0U,
      .peak_working_set_bytes = 0U};
  std::ostringstream zero;
  benchmarker::WriteCsv(zero, result);
  EXPECT_EQ(
      CsvValue(zero.str(), "process_bytes_per_entity", "peak_private_commit"),
      "0");
}

class FailingSimulation {
 public:
  explicit FailingSimulation(const boids::BoidSimulationConfig&) {}
  void Initialize(const boids::SimulationInput&) {}
  void Update(float, std::span<const std::size_t>,
              engine::measurement::TimingRecorder&) {
    throw std::overflow_error("Simulation numeric overflow");
  }
  engine::measurement::StorageReport GetStorageReport() const {
    ADD_FAILURE() << "A failed run must not export storage or results";
    return {};
  }
};

TEST(BenchmarkerExperimentTest,
     PropagatesInitializationWarmupAndMeasurementFailures) {
  for (const auto warmup : {0U, 1U}) {
    EXPECT_THROW(
        static_cast<void>(benchmarker::RunBenchmark<FailingSimulation>(
            {}, {.warmup_steps = warmup, .measurement_steps = 1U}, "test")),
        std::overflow_error);
  }
  class FailingInitialization : public FailingSimulation {
   public:
    using FailingSimulation::FailingSimulation;
    void Initialize(const boids::SimulationInput&) {
      throw std::runtime_error("Initialization failed");
    }
  };
  EXPECT_THROW(
      static_cast<void>(benchmarker::RunBenchmark<FailingInitialization>(
          {}, {.warmup_steps = 0U, .measurement_steps = 1U}, "test")),
      std::runtime_error);
}

TEST(BenchmarkerExperimentTest,
     PublishesOnlyCompleteCsvAndCleansFailedTemporaryOutput) {
  const auto result =
      benchmarker::RunBenchmark<TestSimulation>({},
                                                {.fixed_delta_seconds = 0.025F,
                                                 .warmup_steps = 2U,
                                                 .measurement_steps = 3U},
                                                "test");
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("ecs-benchmark-output-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ignored;
      for (const auto& entry :
           std::filesystem::directory_iterator(path, ignored)) {
        std::filesystem::remove(entry.path(), ignored);
      }
      std::filesystem::remove(path, ignored);
    }
  } cleanup{directory};
  const auto path = directory / "result.csv";
  app::WriteBenchmarkFile(path, result);
  std::ostringstream expected;
  benchmarker::WriteCsv(expected, result);
  std::ifstream file(path, std::ios::binary);
  std::ostringstream contents;
  contents << file.rdbuf();
  EXPECT_EQ(contents.str(), expected.str());
  file.close();
  // Existing output is replaceable, with the same complete document.
  app::WriteBenchmarkFile(path, result);
  const auto blocked = directory / "blocked.csv";
  std::filesystem::create_directory(blocked);
  EXPECT_THROW(app::WriteBenchmarkFile(blocked, result), std::system_error);
  EXPECT_TRUE(std::filesystem::is_directory(blocked));
  EXPECT_THROW(app::WriteBenchmarkFile(path / "nested.csv", result),
               std::filesystem::filesystem_error);
  std::size_t entries = 0U;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    EXPECT_EQ(entry.path().filename().string().find(".tmp-"),
              std::string::npos);
    ++entries;
  }
  EXPECT_EQ(entries, 2U);
  std::ostringstream failed;
  failed.setstate(std::ios::badbit);
  EXPECT_THROW(benchmarker::WriteCsv(failed, result), std::runtime_error);
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
