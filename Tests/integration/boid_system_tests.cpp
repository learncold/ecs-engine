#include <gtest/gtest.h>

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "Boids/Common/boid_parameters.h"
#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/Systems/boid_system.h"
#include "Boids/ECS/Systems/movement_system.h"
#include "Engine/ECS/registry.h"
#include "Engine/Renderer/transform.h"

namespace {

using boids::BoidParameters;
using boids::ecs::Acceleration;
using boids::ecs::BoidSystem;
using boids::ecs::MovementSystem;
using boids::ecs::Velocity;
using engine::renderer::Transform;

constexpr float kTolerance = 0.000001F;

void ExpectVec3Near(const glm::vec3& actual, const glm::vec3& expected) {
  EXPECT_NEAR(actual.x, expected.x, kTolerance);
  EXPECT_NEAR(actual.y, expected.y, kTolerance);
  EXPECT_NEAR(actual.z, expected.z, kTolerance);
}

BoidParameters MakeBoidParameters(float separation_weight = 0.0F,
                                  float alignment_weight = 0.0F,
                                  float cohesion_weight = 0.0F) {
  return BoidParameters{.neighbor_radius = 5.0F,
                        .separation_radius = 1.0F,
                        .separation_weight = separation_weight,
                        .alignment_weight = alignment_weight,
                        .cohesion_weight = cohesion_weight,
                        .preferred_speed = 3.0F,
                        .max_speed = 10.0F,
                        .max_alignment_force = 10.0F};
}

class BoidSystemTest : public testing::Test {
 protected:
  engine::ecs::Entity AddBoid(const glm::vec3& position,
                              const glm::vec3& velocity,
                              const BoidParameters& parameters) {
    const engine::ecs::Entity entity = registry_.Create();
    registry_.Emplace<Transform>(entity, Transform{.position = position});
    registry_.Emplace<Velocity>(entity, Velocity{.value = velocity});
    registry_.Emplace<BoidParameters>(entity, parameters);
    registry_.Emplace<Acceleration>(entity, Acceleration{});
    return entity;
  }

  engine::ecs::Registry registry_;
  BoidSystem boid_system_;
};

TEST_F(BoidSystemTest, ProducesZeroAccelerationWithoutNeighbors) {
  const engine::ecs::Entity self =
      AddBoid({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
              MakeBoidParameters(1.0F, 1.0F, 1.0F));

  boid_system_.Update(registry_, 1.0F);

  ExpectVec3Near(registry_.GetComponent<Acceleration>(self).value, {0.0F, 0.0F, 0.0F});
}

TEST_F(BoidSystemTest, IgnoresBoidsOutsideNeighborRadius) {
  const engine::ecs::Entity self =
      AddBoid({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
              MakeBoidParameters(1.0F, 1.0F, 1.0F));
  AddBoid({5.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F},
          MakeBoidParameters(1.0F, 1.0F, 1.0F));

  boid_system_.Update(registry_, 1.0F);

  ExpectVec3Near(registry_.GetComponent<Acceleration>(self).value, {0.0F, 0.0F, 0.0F});
}

TEST_F(BoidSystemTest, CalculatesSeparationAwayFromNearbyBoid) {
  const engine::ecs::Entity self =
      AddBoid({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, MakeBoidParameters(1.0F));
  AddBoid({0.5F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, MakeBoidParameters(1.0F));

  boid_system_.Update(registry_, 1.0F);

  ExpectVec3Near(registry_.GetComponent<Acceleration>(self).value, {-2.0F, 0.0F, 0.0F});
}

TEST_F(BoidSystemTest, CalculatesAlignmentTowardNeighborVelocity) {
  const engine::ecs::Entity self = AddBoid(
      {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, MakeBoidParameters(0.0F, 1.0F));
  AddBoid({1.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F},
          MakeBoidParameters(0.0F, 1.0F));

  boid_system_.Update(registry_, 1.0F);

  ExpectVec3Near(registry_.GetComponent<Acceleration>(self).value, {-1.0F, 3.0F, 0.0F});
}

TEST_F(BoidSystemTest, LimitsAlignmentSteeringForce) {
  BoidParameters parameters = MakeBoidParameters(0.0F, 1.0F);
  parameters.max_alignment_force = 1.0F;
  const engine::ecs::Entity self =
      AddBoid({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, parameters);
  AddBoid({1.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F}, parameters);

  boid_system_.Update(registry_, 1.0F);

  const glm::vec3 alignment = registry_.GetComponent<Acceleration>(self).value;
  EXPECT_NEAR(glm::length(alignment), 1.0F, kTolerance);
  EXPECT_LT(alignment.x, 0.0F);
  EXPECT_GT(alignment.y, 0.0F);
}

TEST_F(BoidSystemTest, DoesNotBrakeWhenNeighborVelocitiesCancelOut) {
  const BoidParameters parameters = MakeBoidParameters(0.0F, 1.0F);
  const engine::ecs::Entity self =
      AddBoid({0.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}, parameters);
  AddBoid({1.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F}, parameters);
  AddBoid({-1.0F, 0.0F, 0.0F}, {0.0F, -2.0F, 0.0F}, parameters);

  boid_system_.Update(registry_, 1.0F);

  ExpectVec3Near(registry_.GetComponent<Acceleration>(self).value, {0.0F, 0.0F, 0.0F});
}

TEST_F(BoidSystemTest, CalculatesCohesionTowardNeighborPosition) {
  const engine::ecs::Entity self =
      AddBoid({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
              MakeBoidParameters(0.0F, 0.0F, 1.0F));
  AddBoid({1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 0.0F},
          MakeBoidParameters(0.0F, 0.0F, 1.0F));

  boid_system_.Update(registry_, 1.0F);

  ExpectVec3Near(registry_.GetComponent<Acceleration>(self).value, {1.0F, 2.0F, 3.0F});
}

TEST_F(BoidSystemTest, AvoidsNonFiniteAccelerationForOverlappingBoids) {
  const engine::ecs::Entity self =
      AddBoid({1.0F, 2.0F, 3.0F}, {1.0F, 0.0F, 0.0F},
              MakeBoidParameters(1.0F, 1.0F, 1.0F));
  AddBoid({1.0F, 2.0F, 3.0F}, {1.0F, 0.0F, 0.0F},
          MakeBoidParameters(1.0F, 1.0F, 1.0F));

  boid_system_.Update(registry_, 1.0F);

  const glm::vec3 acceleration = registry_.GetComponent<Acceleration>(self).value;
  EXPECT_TRUE(std::isfinite(acceleration.x));
  EXPECT_TRUE(std::isfinite(acceleration.y));
  EXPECT_TRUE(std::isfinite(acceleration.z));
  ExpectVec3Near(acceleration, {2.0F, 0.0F, 0.0F});
}

TEST(BoidMovementPipelineTest, LimitsSpeedBeforeUpdatingPosition) {
  engine::ecs::Registry registry;
  const engine::ecs::Entity entity = registry.Create();
  registry.Emplace<Transform>(entity, Transform{});
  registry.Emplace<Velocity>(entity, Velocity{.value = {3.0F, 4.0F, 0.0F}});
  registry.Emplace<Acceleration>(entity,
                                 Acceleration{.value = {6.0F, 8.0F, 0.0F}});
  registry.Emplace<BoidParameters>(entity, MakeBoidParameters());

  MovementSystem movement_system;
  movement_system.Update(registry, 1.0F);

  ExpectVec3Near(registry.GetComponent<Velocity>(entity).value, {6.0F, 8.0F, 0.0F});
  ExpectVec3Near(registry.GetComponent<Transform>(entity).position, {6.0F, 8.0F, 0.0F});
}

}  // namespace
