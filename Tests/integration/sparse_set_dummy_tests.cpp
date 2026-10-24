#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/boid_simulation.h"
#include "Engine/Measurement/timing_recorder.h"
#include "Engine/Renderer/mesh_renderer.h"
#include "Engine/Renderer/transform.h"

namespace {

using boids::BoidParameters;
using boids::BoidSimulationConfig;
using boids::ObjectKind;
using boids::SimulationInput;
using boids::ecs::Acceleration;
using boids::ecs::BoidSimulation;
using boids::ecs::Velocity;
using engine::ecs::Entity;
using engine::measurement::TimingRecorder;
using engine::renderer::Transform;

constexpr float kTolerance = 1e-5F;
constexpr float kDt = 1.0F / 60.0F;

void ExpectVector(const glm::vec3& actual, const glm::vec3& expected) {
  for (int axis = 0; axis < 3; ++axis) {
    EXPECT_NEAR(actual[axis], expected[axis], kTolerance);
  }
}

void ExpectRotation(const glm::quat& actual, const glm::quat& expected) {
  const auto aligned = glm::dot(actual, expected) < 0.0F ? -expected : expected;
  for (int axis = 0; axis < 4; ++axis) {
    EXPECT_NEAR(actual[axis], aligned[axis], kTolerance);
  }
}

void Step(BoidSimulation& simulation, std::span<const std::size_t> plan = {},
          float dt = kDt) {
  TimingRecorder disabled;
  disabled.set_enabled(false);
  simulation.Update(dt, plan, disabled);
}

void ExpectDummies(const BoidSimulation& simulation,
                   const SimulationInput& input) {
  const auto& registry = simulation.GetRegistry();
  for (const auto& initial : input.initial_objects) {
    if (initial.kind != ObjectKind::Dummy) continue;
    const auto entity = simulation.EntityFor(initial.logical_id);
    ASSERT_TRUE(registry.Has<Transform>(entity));
    ASSERT_TRUE(registry.Has<Velocity>(entity));
    EXPECT_FALSE(registry.Has<Acceleration>(entity));
    EXPECT_FALSE(registry.Has<BoidParameters>(entity));
    EXPECT_FALSE(registry.Has<engine::renderer::MeshRenderer>(entity));
    const auto& transform = *registry.FindComponent<Transform>(entity);
    ExpectVector(transform.position, initial.position);
    ExpectVector(registry.FindComponent<Velocity>(entity)->value,
                 initial.velocity);
    const auto rotation =
        engine::renderer::RotationFromDirection(initial.velocity);
    ExpectRotation(transform.rotation, rotation);
    EXPECT_FLOAT_EQ(transform.scale, 0.7F);
  }
}

void ExpectSameBoids(const BoidSimulation& actual,
                     const BoidSimulation& expected,
                     const SimulationInput& input) {
  for (const auto& initial : input.initial_objects) {
    if (initial.kind != ObjectKind::Boid) continue;
    SCOPED_TRACE(initial.logical_id);
    const auto a = actual.EntityFor(initial.logical_id);
    const auto b = expected.EntityFor(initial.logical_id);
    const auto& ar = actual.GetRegistry();
    const auto& br = expected.GetRegistry();
    const auto& at = *ar.FindComponent<Transform>(a);
    const auto& bt = *br.FindComponent<Transform>(b);
    ExpectVector(at.position, bt.position);
    ExpectVector(ar.FindComponent<Velocity>(a)->value,
                 br.FindComponent<Velocity>(b)->value);
    ExpectVector(ar.FindComponent<Acceleration>(a)->value,
                 br.FindComponent<Acceleration>(b)->value);
    ExpectRotation(at.rotation, bt.rotation);
  }
}

TEST(SparseSetDummyTest, UsesBorrowedOrderStatesAndOnlySimulationComponents) {
  BoidSimulationConfig config;
  config.boid_count = 2U;
  config.dummy_count = 2U;
  const SimulationInput input{
      .initial_objects =
          {{ObjectKind::Dummy, 0U, {100, 200, 300}, {20, -30, 40}},
           {ObjectKind::Boid, 1U, {1, 2, 3}, {0, 0, 0}},
           {ObjectKind::Boid, 2U, {3, 2, 1}, {1, 2, 3}},
           {ObjectKind::Dummy, 3U, {-100, 200, 300}, {-20, 30, 40}}},
      .dummy_steps = {}};
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  const auto& registry = simulation.GetRegistry();
  EXPECT_EQ(registry.EntityCount(), 4U);
  for (const auto& initial : input.initial_objects) {
    const auto entity = simulation.EntityFor(initial.logical_id);
    EXPECT_EQ(entity.Value(), initial.logical_id);
    ExpectVector(registry.FindComponent<Transform>(entity)->position,
                 initial.position);
    ExpectVector(registry.FindComponent<Velocity>(entity)->value,
                 initial.velocity);
    EXPECT_FALSE(registry.Has<engine::renderer::MeshRenderer>(entity));
    if (initial.kind == ObjectKind::Boid) {
      ASSERT_TRUE(registry.Has<Acceleration>(entity));
      ExpectVector(registry.FindComponent<Acceleration>(entity)->value,
                   glm::vec3{0});
      ASSERT_TRUE(registry.Has<BoidParameters>(entity));
      EXPECT_FLOAT_EQ(
          registry.FindComponent<BoidParameters>(entity)->max_alignment_force,
          config.parameters.max_alignment_force);
    }
  }
  Step(simulation);
  ExpectDummies(simulation, input);
}

TEST(SparseSetDummyTest, ValidatesInputBeforeReplacingCurrentState) {
  BoidSimulationConfig config;
  config.boid_count = 1U;
  config.dummy_count = 1U;
  const auto valid = boids::PrepareSimulationInput(config, 0U);
  BoidSimulation simulation(config);
  simulation.Initialize(valid);
  const auto original = simulation.EntityFor(0U);
  for (int scenario = 0; scenario < 6; ++scenario) {
    auto invalid = valid;
    switch (scenario) {
      case 0:
        invalid.initial_objects.pop_back();
        break;
      case 1:
        invalid.initial_objects[1].logical_id = 0U;
        break;
      case 2:
        invalid.initial_objects[1].kind = ObjectKind::Boid;
        break;
      case 3:
        invalid.initial_objects[1].kind = static_cast<ObjectKind>(99);
        break;
      case 4:
        invalid.initial_objects[0].position.x =
            std::numeric_limits<float>::infinity();
        break;
      case 5:
        invalid.initial_objects[1].velocity.z =
            std::numeric_limits<float>::quiet_NaN();
        break;
    }
    EXPECT_THROW(simulation.Initialize(invalid), std::invalid_argument);
    EXPECT_EQ(simulation.EntityFor(0U), original);
  }
  EXPECT_NO_THROW(Step(simulation));
  config.dummy_count = std::numeric_limits<std::size_t>::max();
  EXPECT_THROW(static_cast<void>(BoidSimulation{config}), std::overflow_error);
}

TEST(SparseSetDummyTest, RequiresInitializationPlanAndValidDeltaTime) {
  BoidSimulationConfig config;
  config.boid_count = 1U;
  config.dummy_count = 3U;
  config.dummy_change_percent = 50U;
  BoidSimulation simulation(config);
  EXPECT_THROW(Step(simulation), std::logic_error);
  EXPECT_THROW(static_cast<void>(simulation.EntityFor(0U)), std::out_of_range);
  simulation.Initialize();
  EXPECT_THROW(simulation.Update(kDt), std::logic_error);
  TimingRecorder disabled;
  disabled.set_enabled(false);
  EXPECT_THROW(simulation.Update(kDt, disabled), std::logic_error);
  for (const float dt : {-1.0F, std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()}) {
    EXPECT_THROW(Step(simulation, {}, dt), std::invalid_argument);
  }
  EXPECT_NO_THROW(Step(simulation, {}, 0.0F));
  EXPECT_THROW(static_cast<void>(simulation.EntityFor(4U)), std::out_of_range);
  simulation.GetRegistry().Destroy(simulation.EntityFor(0U));
  EXPECT_THROW(static_cast<void>(simulation.EntityFor(0U)), std::out_of_range);

  config.dummy_change_percent = 1U;  // floor(3 * 1 / 100) == 0.
  BoidSimulation floor_zero(config);
  floor_zero.Initialize();
  EXPECT_NO_THROW(floor_zero.Update(kDt));
  EXPECT_NO_THROW(floor_zero.Update(kDt, disabled));
}

TEST(SparseSetDummyTest, ReinitializationInvalidatesHandlesAndRefreshesCache) {
  BoidSimulationConfig config;
  config.boid_count = 4U;
  config.dummy_count = 3U;
  config.dummy_change_percent = 100U;
  auto first = boids::PrepareSimulationInput(config, 1U);
  BoidSimulation simulation(config);
  simulation.Initialize();
  Step(simulation, first.dummy_steps[0]);
  std::vector<Entity> old;
  for (std::size_t id = 0; id < first.initial_objects.size(); ++id)
    old.push_back(simulation.EntityFor(id));
  config.random_seed += 1U;
  const auto second = boids::PrepareSimulationInput(config, 1U);
  simulation.Initialize(second);
  BoidSimulation fresh(config);
  fresh.Initialize(second);
  for (const auto entity : old)
    EXPECT_FALSE(simulation.GetRegistry().IsAlive(entity));
  Step(simulation, second.dummy_steps[0]);
  Step(fresh, second.dummy_steps[0]);
  ExpectSameBoids(simulation, fresh, second);
  ExpectDummies(simulation, second);
  const auto report = simulation.GetStorageReport();
  for (const auto& entry : report.entries()) {
    EXPECT_FALSE(entry.name.starts_with("input."));
  }
}

TEST(SparseSetDummyTest,
     RejectsInvalidPlansAndRequiresRecoveryAfterPartialChanges) {
  BoidSimulationConfig config;
  config.boid_count = 1U;
  config.dummy_count = 2U;
  const auto input = boids::PrepareSimulationInput(config, 0U);
  BoidSimulation simulation(config);
  const std::vector<std::vector<std::size_t>> plans{
      {1U, 1U}, {1U, 3U}, {1U, 0U}};
  for (const auto& plan : plans) {
    simulation.Initialize(input);
    EXPECT_THROW(Step(simulation, plan), std::exception);
    EXPECT_THROW(Step(simulation), std::logic_error);
    simulation.Initialize(input);
    EXPECT_NO_THROW(Step(simulation));
    ExpectDummies(simulation, input);
  }
}

TEST(SparseSetDummyTest, RecreationRestoresOriginalInputInsteadOfDeletedState) {
  BoidSimulationConfig config;
  config.boid_count = 2U;
  config.dummy_count = 2U;
  config.dummy_change_percent = 100U;
  const auto input = boids::PrepareSimulationInput(config, 1U);
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  for (const auto id : input.dummy_steps[0]) {
    const auto entity = simulation.EntityFor(id);
    simulation.GetRegistry().FindComponent<Transform>(entity)->position =
        glm::vec3{999.0F};
    simulation.GetRegistry().FindComponent<Velocity>(entity)->value =
        glm::vec3{999.0F};
  }
  Step(simulation, input.dummy_steps[0]);
  ExpectDummies(simulation, input);
}

struct ChurnCase {
  std::size_t boids;
  std::size_t dummies;
  std::uint32_t percent;
};
class SparseSetChurnTest : public testing::TestWithParam<ChurnCase> {};

TEST_P(SparseSetChurnTest,
       RecreatesAllSelectedDummiesBeforeQueriesAndRecordsFourSections) {
  const auto scenario = GetParam();
  BoidSimulationConfig config;
  config.boid_count = scenario.boids;
  config.dummy_count = scenario.dummies;
  config.dummy_change_percent = scenario.percent;
  constexpr std::size_t kWarmup = 3U;
  constexpr std::size_t kMeasured = 9U;
  auto input = boids::PrepareSimulationInput(config, kWarmup + kMeasured);
  // Unsorted plans must be honored as supplied, including repeated batches.
  for (auto& plan : input.dummy_steps) std::reverse(plan.begin(), plan.end());
  for (auto& initial : input.initial_objects) {
    if (initial.kind == ObjectKind::Dummy) {
      initial.position = {100, -200, 300};
      initial.velocity = {20, -30, 40};
    }
  }
  const auto snapshot = input;
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  TimingRecorder timing({"entity_changes", "query_preparation",
                         "neighbor_calculation", "state_update", "unmeasured"});
  timing.set_enabled(false);
  for (std::size_t step = 0; step < input.dummy_steps.size(); ++step) {
    SCOPED_TRACE(step);
    if (step == kWarmup) timing.begin_measurement(kMeasured);
    std::vector<Entity> previous;
    for (std::size_t id = 0; id < input.initial_objects.size(); ++id)
      previous.push_back(simulation.EntityFor(id));
    const auto& plan = input.dummy_steps[step];
    if (step >= kWarmup) timing.begin_step_measurement();
    simulation.Update(kDt, plan, timing);
    if (step >= kWarmup) timing.end_step_measurement();
    EXPECT_EQ(simulation.GetRegistry().EntityCount(),
              input.initial_objects.size());
    // LIFO reuse proves all deletions preceded all creations in list order.
    for (std::size_t i = 0; i < plan.size(); ++i) {
      EXPECT_EQ(simulation.EntityFor(plan[i]).Value(),
                previous[plan[plan.size() - i - 1U]].Value());
    }
    for (std::size_t id = 0; id < previous.size(); ++id) {
      const bool replaced =
          std::find(plan.begin(), plan.end(), id) != plan.end();
      EXPECT_EQ(simulation.GetRegistry().IsAlive(previous[id]), !replaced);
      EXPECT_EQ(simulation.EntityFor(id) == previous[id], !replaced);
    }
    ExpectDummies(simulation, input);
  }
  const auto result = timing.result();
  EXPECT_EQ(result.measured_steps, kMeasured);
  ASSERT_EQ(result.sections.size(), 5U);
  for (const auto& section : result.sections) {
    const bool measured = section.name != "unmeasured";
    EXPECT_EQ(section.recorded_steps, measured ? kMeasured : 0U);
    EXPECT_EQ(section.mean_ms_per_step.has_value(), measured);
    for (const auto& sample : section.samples) {
      EXPECT_EQ(sample.recorded, measured);
      const bool zero =
          !measured ||
          (section.name == "entity_changes" &&
           scenario.dummies * scenario.percent / 100U == 0U) ||
          (scenario.boids == 0U && (section.name == "neighbor_calculation" ||
                                    section.name == "state_update"));
      EXPECT_EQ(sample.calls, zero ? 0U : 1U);
      if (zero) EXPECT_DOUBLE_EQ(sample.elapsed_ms, 0.0);
    }
  }
  EXPECT_EQ(input.dummy_steps, snapshot.dummy_steps);
  for (std::size_t i = 0; i < input.initial_objects.size(); ++i) {
    ExpectVector(input.initial_objects[i].position,
                 snapshot.initial_objects[i].position);
    ExpectVector(input.initial_objects[i].velocity,
                 snapshot.initial_objects[i].velocity);
    EXPECT_EQ(input.initial_objects[i].logical_id,
              snapshot.initial_objects[i].logical_id);
    EXPECT_EQ(input.initial_objects[i].kind, snapshot.initial_objects[i].kind);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CountsAndPercentages, SparseSetChurnTest,
    testing::Values(ChurnCase{0, 0, 0}, ChurnCase{4, 0, 100},
                    ChurnCase{0, 5, 100}, ChurnCase{0, 7, 40},
                    ChurnCase{5, 3, 0}, ChurnCase{7, 3, 40},
                    ChurnCase{5, 3, 100}, ChurnCase{3, 1, 1},
                    ChurnCase{3, 5, 20}));

TEST(SparseSetDummyTest,
     DummyStateAndChurnDoNotAffectBoidsAcrossWarmupAndMeasurement) {
  BoidSimulationConfig config;
  config.boid_count = 17U;
  config.dummy_count = 13U;
  config.spawn_half_extent = 2.0F;
  config.boundary_half_extent = 3.0F;
  constexpr std::size_t kWarmup = 100U;
  constexpr std::size_t kMeasured = 1000U;
  const auto baseline_input =
      boids::PrepareSimulationInput(config, kWarmup + kMeasured);
  for (const std::uint32_t percent : {0U, 40U, 100U}) {
    SCOPED_TRACE(percent);
    config.dummy_change_percent = percent;
    auto changing_input =
        boids::PrepareSimulationInput(config, kWarmup + kMeasured);
    for (auto& initial : changing_input.initial_objects) {
      if (initial.kind == ObjectKind::Dummy) {
        initial.position = glm::vec3{0.0F};
        initial.velocity = {100.0F, -200.0F, 300.0F};
      }
    }
    BoidSimulation baseline(config);
    BoidSimulation changing(config);
    baseline.Initialize(baseline_input);
    changing.Initialize(changing_input);
    TimingRecorder timing({"entity_changes", "query_preparation",
                           "neighbor_calculation", "state_update"});
    timing.set_enabled(false);
    for (std::size_t step = 0; step < kWarmup + kMeasured; ++step) {
      SCOPED_TRACE(step);
      Step(baseline);
      if (step == kWarmup) timing.begin_measurement(kMeasured);
      if (step >= kWarmup) timing.begin_step_measurement();
      changing.Update(kDt, changing_input.dummy_steps[step], timing);
      if (step >= kWarmup) timing.end_step_measurement();
      ExpectSameBoids(changing, baseline, baseline_input);
      ExpectDummies(changing, changing_input);
      ASSERT_FALSE(testing::Test::HasFailure());
    }
    EXPECT_EQ(timing.result().measured_steps, kMeasured);
  }
}

TEST(SparseSetDummyTest, ReportsMappingSystemsCacheAndOnlyOwnedInputOnce) {
  BoidSimulationConfig config;
  config.boid_count = 3U;
  config.dummy_count = 2U;
  const auto input = boids::PrepareSimulationInput(config, 0U);
  BoidSimulation simulation(config);
  for (const bool own_input : {false, true}) {
    if (own_input)
      simulation.Initialize();
    else
      simulation.Initialize(input);
    Step(simulation);
    const auto report = simulation.GetStorageReport();
    std::set<std::string> names;
    for (const auto& entry : report.entries()) {
      EXPECT_TRUE(names.insert(entry.name).second);
      if (entry.name == "entities.logical_ids")
        EXPECT_EQ(entry.size_bytes, 5U * sizeof(Entity));
      if (entry.name == "neighbors.cached_entries")
        EXPECT_GT(entry.size_bytes.value(), 0U);
      if (entry.name == "input.initial_objects")
        EXPECT_EQ(entry.size_bytes, 5U * sizeof(boids::InitialObjectState));
    }
    EXPECT_TRUE(names.contains("entities.logical_ids"));
    EXPECT_TRUE(names.contains("neighbors.cached_entries"));
    EXPECT_TRUE(names.contains("systems.entries"));
    EXPECT_TRUE(names.contains("state_systems.entries"));
    EXPECT_EQ(names.contains("input.object"), own_input);
    EXPECT_EQ(names.contains("input.initial_objects"), own_input);
    EXPECT_FALSE(
        report.complete());  // Existing opaque hash node/bucket storage.
    EXPECT_GE(report.capacity_bytes(), report.size_bytes());
  }
}

}  // namespace
