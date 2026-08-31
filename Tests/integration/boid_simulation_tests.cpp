#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/vec3.hpp>

#include "Engine/ECS/registry.h"
#include "Sandbox/Boids/ECS/boid_simulation.h"
#include "Sandbox/Boids/ECS/Components/acceleration.h"
#include "Sandbox/Boids/ECS/Components/boid.h"
#include "Sandbox/Boids/ECS/Components/transform.h"
#include "Sandbox/Boids/ECS/Components/velocity.h"
#include "Sandbox/Boids/ECS/Systems/boundary_system.h"

namespace {

struct BoidState {
  glm::vec3 position;
  glm::vec3 velocity;
};

void ExpectVec3Near(const glm::vec3& actual, const glm::vec3& expected) {
  constexpr float kTolerance = 0.000001F;
  EXPECT_NEAR(actual.x, expected.x, kTolerance);
  EXPECT_NEAR(actual.y, expected.y, kTolerance);
  EXPECT_NEAR(actual.z, expected.z, kTolerance);
}

std::vector<BoidState> CollectStates(engine::ecs::Registry& registry) {
  std::vector<BoidState> states;
  registry.CreateView<Transform, Velocity>().Each(
      [&states](const Transform& transform, const Velocity& velocity) {
        states.push_back(
            BoidState{.position = transform.position,
                      .velocity = velocity.value});
      });
  return states;
}

TEST(BoidSimulationTest, CreatesRequestedNumberOfCompleteBoids) {
  BoidSimulationConfig config;
  config.boid_count = 12U;
  BoidSimulation simulation(config);

  simulation.Initialize();

  engine::ecs::Registry& registry = simulation.GetRegistry();
  EXPECT_EQ(registry.EntityCount(), config.boid_count);

  std::size_t complete_boid_count = 0U;
  registry.CreateView<Transform, Velocity, Acceleration, Boid>().Each(
      [&complete_boid_count](const Transform&, const Velocity&,
                             const Acceleration&, const Boid&) {
        ++complete_boid_count;
      });
  EXPECT_EQ(complete_boid_count, config.boid_count);
}

TEST(BoidSimulationTest, ReproducesInitialStateWithSameSeed) {
  BoidSimulationConfig config;
  config.boid_count = 8U;
  config.random_seed = 12345U;
  BoidSimulation first(config);
  BoidSimulation second(config);

  first.Initialize();
  second.Initialize();

  const std::vector<BoidState> first_states =
      CollectStates(first.GetRegistry());
  const std::vector<BoidState> second_states =
      CollectStates(second.GetRegistry());

  ASSERT_EQ(first_states.size(), second_states.size());
  for (std::size_t index = 0; index < first_states.size(); ++index) {
    ExpectVec3Near(first_states[index].position, second_states[index].position);
    ExpectVec3Near(first_states[index].velocity, second_states[index].velocity);
  }
}

TEST(BoidSimulationTest, ZeroDeltaTimePreservesPositionAndVelocity) {
  BoidSimulationConfig config;
  config.boid_count = 6U;
  BoidSimulation simulation(config);
  simulation.Initialize();
  const std::vector<BoidState> before = CollectStates(simulation.GetRegistry());

  simulation.Update(0.0F);

  const std::vector<BoidState> after = CollectStates(simulation.GetRegistry());
  ASSERT_EQ(before.size(), after.size());
  for (std::size_t index = 0; index < before.size(); ++index) {
    ExpectVec3Near(after[index].position, before[index].position);
    ExpectVec3Near(after[index].velocity, before[index].velocity);
  }
}

TEST(BoidSimulationTest, KeepsStateFiniteAndInsideBoundaryAcrossFrames) {
  BoidSimulationConfig config;
  config.boid_count = 20U;
  config.random_seed = 9876U;
  config.spawn_half_extent = 2.0F;
  config.boundary_half_extent = 3.0F;
  BoidSimulation simulation(config);
  simulation.Initialize();

  constexpr float kDeltaSeconds = 1.0F / 60.0F;
  constexpr int kFrameCount = 120;
  for (int frame = 0; frame < kFrameCount; ++frame) {
    simulation.Update(kDeltaSeconds);
  }

  simulation.GetRegistry()
      .CreateView<Transform, Velocity, Acceleration>()
      .Each([&config](const Transform& transform, const Velocity& velocity,
                     const Acceleration& acceleration) {
        for (int axis = 0; axis < 3; ++axis) {
          EXPECT_TRUE(std::isfinite(transform.position[axis]));
          EXPECT_TRUE(std::isfinite(velocity.value[axis]));
          EXPECT_TRUE(std::isfinite(acceleration.value[axis]));
          EXPECT_LE(std::abs(transform.position[axis]),
                    config.boundary_half_extent);
        }
      });
}

TEST(BoundarySystemTest, ClampsPositionAndReflectsOutwardVelocity) {
  engine::ecs::Registry registry;
  const engine::ecs::Entity entity = registry.Create();
  registry.Emplace<Transform>(
      entity, Transform{.position = {3.0F, -4.0F, 1.0F}});
  registry.Emplace<Velocity>(entity,
                             Velocity{.value = {1.0F, -2.0F, 3.0F}});
  BoundarySystem boundary_system(2.0F);

  boundary_system.Update(registry, 0.0F);

  ExpectVec3Near(registry.Get<Transform>(entity).position,
                 {2.0F, -2.0F, 1.0F});
  ExpectVec3Near(registry.Get<Velocity>(entity).value,
                 {-1.0F, 2.0F, 3.0F});
}

TEST(BoidSimulationTest, RejectsNegativeDeltaTime) {
  BoidSimulation simulation;
  simulation.Initialize();

  EXPECT_THROW(simulation.Update(-0.01F), std::invalid_argument);
}

}  // namespace
