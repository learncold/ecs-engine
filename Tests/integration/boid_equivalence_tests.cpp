#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Boids/Archetype/boid_simulation.h"
#include "Boids/Common/boid_simulation_config.h"
#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/boid_simulation.h"
#include "Boids/OOP/boid_object.h"
#include "Boids/OOP/boid_simulation.h"
#include "Engine/Renderer/transform.h"

namespace {

constexpr float kTolerance = 0.00001F;
using boids::ecs::Acceleration;
using boids::ecs::Velocity;
using engine::renderer::Transform;

void ExpectVec3Near(const glm::vec3& actual, const glm::vec3& expected) {
  for (int axis = 0; axis < 3; ++axis) {
    SCOPED_TRACE(axis);
    EXPECT_NEAR(actual[axis], expected[axis], kTolerance);
  }
}

void ExpectRotationNear(const glm::quat& actual, const glm::quat& expected) {
  EXPECT_NEAR(glm::dot(actual, actual), 1.0F, kTolerance);
  EXPECT_NEAR(glm::dot(expected, expected), 1.0F, kTolerance);
  // q and -q represent the same rotation.
  const glm::quat aligned =
      glm::dot(actual, expected) < 0.0F ? -actual : actual;
  for (int component = 0; component < 4; ++component) {
    EXPECT_NEAR(aligned[component], expected[component], kTolerance);
  }
}

void CompareSimulations(const boids::BoidSimulationConfig& config,
                        float delta_seconds = 1.0F / 60.0F,
                        bool use_borrowed_input = false) {
  boids::ecs::BoidSimulation ecs_simulation(config);
  boids::oop::BoidSimulation oop_simulation(config);
  ecs_simulation.Initialize();

  auto& registry = ecs_simulation.GetRegistry();
  std::vector<engine::ecs::Entity> entities;
  registry.CreateView<Transform, Velocity, Acceleration>().Each(
      [&](engine::ecs::Entity entity, const Transform&, const Velocity&,
          const Acceleration&) { entities.push_back(entity); });
  // A fresh registry allocates IDs in creation order, matching OOP indices.
  std::sort(entities.begin(), entities.end(),
            [](auto left, auto right) { return left.Value() < right.Value(); });
  boids::SimulationInput input;
  if (use_borrowed_input) {
    for (const auto entity : entities) {
      input.initial_objects.push_back(
          {.kind = boids::ObjectKind::Boid,
           .logical_id = input.initial_objects.size(),
           .position = registry.FindComponent<Transform>(entity)->position,
           .velocity = registry.FindComponent<Velocity>(entity)->value});
    }
    oop_simulation.Initialize(input);
  } else {
    oop_simulation.Initialize();
  }
  ASSERT_EQ(entities.size(), config.boid_count);
  ASSERT_EQ(oop_simulation.Objects().size(), config.boid_count);

  for (int step = 0; step <= 100; ++step) {
    SCOPED_TRACE(::testing::Message() << "step=" << step);
    if (step > 0) {
      ecs_simulation.Update(delta_seconds);
      oop_simulation.Update(delta_seconds);
    }
    for (std::size_t index = 0; index < entities.size(); ++index) {
      SCOPED_TRACE(::testing::Message() << "boid=" << index);
      const auto entity = entities[index];
      const auto& base = *oop_simulation.Objects()[index];
      ASSERT_EQ(base.Kind(), boids::ObjectKind::Boid);
      EXPECT_EQ(base.LogicalId(), index);
      const auto& object = static_cast<const boids::oop::BoidObject&>(base);
      const auto* transform = registry.FindComponent<Transform>(entity);
      ASSERT_NE(transform, nullptr);
      {
        SCOPED_TRACE("position");
        ExpectVec3Near(object.Position(), transform->position);
      }
      {
        SCOPED_TRACE("velocity");
        ExpectVec3Near(object.Velocity(),
                       registry.FindComponent<Velocity>(entity)->value);
      }
      {
        SCOPED_TRACE("acceleration");
        ExpectVec3Near(object.Acceleration(),
                       registry.FindComponent<Acceleration>(entity)->value);
      }
      ExpectRotationNear(object.GetTransform().rotation, transform->rotation);
      EXPECT_FLOAT_EQ(object.GetTransform().scale, transform->scale);
    }
    if (::testing::Test::HasFailure()) {
      return;
    }
  }
}

TEST(BoidEquivalenceTest, MatchesInitialStateAndEveryStepForMultipleSeeds) {
  for (std::uint32_t seed : {0U, 42U, 12345U}) {
    SCOPED_TRACE(::testing::Message() << "seed=" << seed);
    boids::BoidSimulationConfig config;
    config.boid_count = 24U;
    config.random_seed = seed;
    CompareSimulations(config);
  }
}

TEST(BoidEquivalenceTest, MatchesBorrowedInputForMultipleSeedsAndBoundaries) {
  for (std::uint32_t seed : {0U, 42U, 12345U}) {
    SCOPED_TRACE(::testing::Message() << "seed=" << seed);
    boids::BoidSimulationConfig config;
    config.boid_count = 24U;
    config.random_seed = seed;
    CompareSimulations(config, 1.0F / 60.0F, true);
    config.spawn_half_extent = 1.0F;
    config.boundary_half_extent = 1.0F;
    config.initial_speed = 10.0F;
    config.parameters.max_speed = 3.0F;
    CompareSimulations(config, 0.05F, true);
  }
}

TEST(BoidEquivalenceTest, MatchesWithSpeedLimitingAndBoundaryReflections) {
  boids::BoidSimulationConfig config;
  config.boid_count = 12U;
  config.spawn_half_extent = 1.0F;
  config.boundary_half_extent = 1.0F;
  config.initial_speed = 10.0F;
  config.parameters.max_speed = 3.0F;
  CompareSimulations(config, 0.05F);
}

TEST(BoidEquivalenceTest, MatchesWithoutNeighbors) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest, MatchesWithCoincidentPositions) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.spawn_half_extent = 0.0F;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest, MatchesWithZeroSpeed) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.initial_speed = 0.0F;
  config.parameters.max_speed = 0.0F;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest, MatchesWithZeroDeltaTime) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  CompareSimulations(config, 0.0F);
}

TEST(BoidEquivalenceTest, MatchesWithLargeFiniteInitialSpeed) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.initial_speed = 1e20F;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest, OopAndArchetypeMatchDuringDummyChurn) {
  constexpr std::size_t kWarmup = 100U;
  constexpr std::size_t kMeasured = 1000U;
  constexpr float kDt = 1.0F / 60.0F;
  for (const std::uint32_t seed : {0U, 42U, 12345U}) {
    for (const std::uint32_t percent : {0U, 40U, 100U}) {
      SCOPED_TRACE(::testing::Message() << seed << ", " << percent);
      boids::BoidSimulationConfig config;
      config.boid_count = 17U;
      config.dummy_count = 23U;
      config.random_seed = seed;
      config.dummy_change_percent = percent;
      config.spawn_half_extent = 2.0F;
      config.boundary_half_extent = 3.0F;
      const auto input =
          boids::PrepareSimulationInput(config, kWarmup + kMeasured);
      boids::oop::BoidSimulation oop(config);
      boids::archetype::BoidSimulation archetype(config);
      oop.Initialize(input);
      archetype.Initialize(input);
      engine::measurement::TimingRecorder disabled;
      disabled.set_enabled(false);
      for (std::size_t step = 0; step < input.dummy_steps.size(); ++step) {
        SCOPED_TRACE(step);
        oop.Update(kDt, input.dummy_steps[step], disabled);
        archetype.Update(kDt, input.dummy_steps[step], disabled);
        const auto& table = *archetype.GetRegistry()
                                 .FindTable<Transform, Velocity, Acceleration,
                                            boids::BoidParameters>();
        const auto entities = table.Entities();
        ASSERT_EQ(oop.Objects().size(), input.initial_objects.size());
        ASSERT_EQ(archetype.GetRegistry().EntityCount(),
                  input.initial_objects.size());
        for (const auto& base : oop.Objects()) {
          if (base->Kind() != boids::ObjectKind::Boid) continue;
          SCOPED_TRACE(base->LogicalId());
          const auto found = std::find(entities.begin(), entities.end(),
                                       archetype.EntityFor(base->LogicalId()));
          ASSERT_NE(found, entities.end());
          const auto row = static_cast<std::size_t>(found - entities.begin());
          const auto& object =
              static_cast<const boids::oop::BoidObject&>(*base);
          const auto& transform = table.Column<Transform>()[row];
          ExpectVec3Near(object.Position(), transform.position);
          ExpectVec3Near(object.Velocity(),
                         table.Column<Velocity>()[row].value);
          ExpectVec3Near(object.Acceleration(),
                         table.Column<Acceleration>()[row].value);
          ExpectRotationNear(object.GetTransform().rotation,
                             transform.rotation);
          EXPECT_FLOAT_EQ(object.GetTransform().scale, transform.scale);
        }
        ASSERT_FALSE(::testing::Test::HasFailure());
      }
    }
  }
}

// All implementations reject overflow and require reinitialization afterward.
void ExpectArithmeticOverflow(const boids::BoidSimulationConfig& config,
                              const boids::SimulationInput& input, float dt) {
  boids::archetype::BoidSimulation archetype(config);
  archetype.Initialize(input);
  engine::measurement::TimingRecorder timing(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  timing.begin_measurement(1U);
  timing.begin_step_measurement();
  EXPECT_THROW(archetype.Update(dt, {}, timing), std::overflow_error);
  EXPECT_THROW(timing.end_step_measurement(), std::logic_error);
  EXPECT_THROW(static_cast<void>(timing.result()), std::logic_error);
  engine::measurement::TimingRecorder disabled;
  disabled.set_enabled(false);
  EXPECT_THROW(archetype.Update(dt, {}, disabled), std::logic_error);

  boids::oop::BoidSimulation oop(config);
  oop.Initialize(input);
  engine::measurement::TimingRecorder oop_timing(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  oop_timing.begin_measurement(1U);
  oop_timing.begin_step_measurement();
  EXPECT_THROW(oop.Update(dt, {}, oop_timing), std::overflow_error);
  EXPECT_THROW(oop_timing.end_step_measurement(), std::logic_error);
  EXPECT_THROW(static_cast<void>(oop_timing.result()), std::logic_error);
  EXPECT_THROW(oop.Update(dt, {}, disabled), std::logic_error);
  auto reset_input = input;
  for (auto& initial : reset_input.initial_objects) {
    initial.position = glm::vec3{0.0F};
    initial.velocity = glm::vec3{0.0F};
  }
  oop.Initialize(reset_input);
  EXPECT_NO_THROW(oop.Update(0.0F, {}, disabled));

  boids::ecs::BoidSimulation ecs(config);
  ecs.Initialize(input);
  engine::measurement::TimingRecorder ecs_timing(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  ecs_timing.begin_measurement(1U);
  ecs_timing.begin_step_measurement();
  EXPECT_THROW(ecs.Update(dt, {}, ecs_timing), std::overflow_error);
  EXPECT_THROW(ecs_timing.end_step_measurement(), std::logic_error);
  EXPECT_THROW(static_cast<void>(ecs_timing.result()), std::logic_error);
  EXPECT_THROW(ecs.Update(dt, {}, disabled), std::logic_error);
  ecs.Initialize(reset_input);
  EXPECT_NO_THROW(ecs.Update(0.0F, {}, disabled));
}

TEST(BoidEquivalenceTest, RejectsPositionOverflowBeforeBoundaryClamping) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  config.boundary_half_extent = 3.4e38F;
  config.parameters.max_speed = 1e38F;
  const boids::SimulationInput input{
      .initial_objects =
          {{boids::ObjectKind::Boid, 0U, {1e38F, 0, 0}, {-1e38F, 0, 0}}},
      .dummy_steps = {}};
  // The exact sum would still be inside the boundary, but the intermediate
  // float product overflows. Reject it instead of inventing a collision.
  ExpectArithmeticOverflow(config, input, 4.0F);
}

TEST(BoidEquivalenceTest, RejectsVelocityOverflowEvenWithZeroSpeedLimit) {
  boids::BoidSimulationConfig config;
  config.boid_count = 2U;
  config.parameters.cohesion_weight = 1e38F;
  config.parameters.max_speed = 0.0F;
  const boids::SimulationInput input{
      .initial_objects = {{boids::ObjectKind::Boid, 0U, {0, 0, 0}, {0, 0, 0}},
                          {boids::ObjectKind::Boid, 1U, {2, 0, 0}, {0, 0, 0}}},
      .dummy_steps = {}};
  ExpectArithmeticOverflow(config, input, 2.0F);
}

TEST(BoidEquivalenceTest, RejectsNeighborOverflowEvenWithZeroAlignmentForce) {
  boids::BoidSimulationConfig config;
  config.boid_count = 3U;
  config.parameters.max_alignment_force = 0.0F;
  config.parameters.separation_weight = 0.0F;
  config.parameters.cohesion_weight = 0.0F;
  const boids::SimulationInput input{
      .initial_objects =
          {{boids::ObjectKind::Boid, 0U, {0, 0, 0}, {2e38F, 0, 0}},
           {boids::ObjectKind::Boid, 1U, {1, 0, 0}, {2e38F, 0, 0}},
           {boids::ObjectKind::Boid, 2U, {2, 0, 0}, {2e38F, 0, 0}}},
      .dummy_steps = {}};
  ExpectArithmeticOverflow(config, input, 0.01F);
}

TEST(BoidEquivalenceTest, LegacySimulationsRejectInvalidConfigAndDt) {
  for (const float invalid : {-1.0F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
    boids::BoidSimulationConfig config;
    config.initial_speed = invalid;
    EXPECT_THROW(static_cast<void>(boids::ecs::BoidSimulation{config}),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(boids::oop::BoidSimulation{config}),
                 std::invalid_argument);
    config = {};
    config.parameters.max_speed = invalid;
    EXPECT_THROW(static_cast<void>(boids::ecs::BoidSimulation{config}),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(boids::oop::BoidSimulation{config}),
                 std::invalid_argument);
    config = {};
    config.boid_count = 1U;
    boids::ecs::BoidSimulation ecs(config);
    boids::oop::BoidSimulation oop(config);
    ecs.Initialize();
    oop.Initialize();
    const auto before = oop.Objects()[0]->Position();
    EXPECT_THROW(ecs.Update(invalid), std::invalid_argument);
    EXPECT_THROW(oop.Update(invalid), std::invalid_argument);
    ExpectVec3Near(oop.Objects()[0]->Position(), before);
    EXPECT_NO_THROW(ecs.Update(0.01F));
    EXPECT_NO_THROW(oop.Update(0.01F));
  }
}

}  // namespace
