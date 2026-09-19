#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "Boids/Common/boid_simulation_config.h"
#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/boid_simulation.h"
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
  const glm::quat aligned = glm::dot(actual, expected) < 0.0F ? -actual : actual;
  for (int component = 0; component < 4; ++component) {
    EXPECT_NEAR(aligned[component], expected[component], kTolerance);
  }
}

void CompareSimulations(const boids::BoidSimulationConfig& config,
                        float delta_seconds = 1.0F / 60.0F) {
  boids::ecs::BoidSimulation ecs_simulation(config);
  boids::oop::BoidSimulation oop_simulation(config);
  ecs_simulation.Initialize();
  oop_simulation.Initialize();

  auto& registry = ecs_simulation.GetRegistry();
  std::vector<engine::ecs::Entity> entities;
  registry.CreateView<Transform, Velocity, Acceleration>().Each(
      [&](engine::ecs::Entity entity, const Transform&, const Velocity&,
          const Acceleration&) { entities.push_back(entity); });
  // A fresh registry allocates IDs in creation order, matching OOP indices.
  std::sort(entities.begin(), entities.end(), [](auto left, auto right) {
    return left.Value() < right.Value();
  });
  ASSERT_EQ(entities.size(), config.boid_count);
  ASSERT_EQ(oop_simulation.Boids().size(), config.boid_count);

  for (int step = 0; step <= 100; ++step) {
    SCOPED_TRACE(::testing::Message() << "step=" << step);
    if (step > 0) {
      ecs_simulation.Update(delta_seconds);
      oop_simulation.Update(delta_seconds);
    }
    for (std::size_t index = 0; index < entities.size(); ++index) {
      SCOPED_TRACE(::testing::Message() << "boid=" << index);
      const auto entity = entities[index];
      const auto& object = oop_simulation.Boids()[index];
      const auto& transform = registry.GetComponent<Transform>(entity);
      {
        SCOPED_TRACE("position");
        ExpectVec3Near(object.Position(), transform.position);
      }
      {
        SCOPED_TRACE("velocity");
        ExpectVec3Near(object.Velocity(), registry.GetComponent<Velocity>(entity).value);
      }
      {
        SCOPED_TRACE("acceleration");
        ExpectVec3Near(object.Acceleration(),
                       registry.GetComponent<Acceleration>(entity).value);
      }
      ExpectRotationNear(object.GetTransform().rotation, transform.rotation);
      EXPECT_FLOAT_EQ(object.GetTransform().scale, transform.scale);
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

}  // namespace
