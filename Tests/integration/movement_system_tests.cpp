#include <gtest/gtest.h>

#include <glm/vec3.hpp>

#include "Engine/ECS/registry.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/Systems/movement_system.h"
#include "Engine/Renderer/transform.h"

namespace {

using boids::ecs::MovementSystem;
using boids::ecs::Velocity;
using engine::renderer::Transform;

void ExpectVec3Near(const glm::vec3& actual, const glm::vec3& expected) {
  constexpr float kTolerance = 0.000001F;
  EXPECT_NEAR(actual.x, expected.x, kTolerance);
  EXPECT_NEAR(actual.y, expected.y, kTolerance);
  EXPECT_NEAR(actual.z, expected.z, kTolerance);
}

class MovementSystemTest : public testing::Test {
 protected:
  void SetUp() override {
    moving_entity_ = registry_.Create();
    transform_only_entity_ = registry_.Create();
    velocity_only_entity_ = registry_.Create();

    registry_.Emplace<Transform>(moving_entity_,
                                 Transform{.position = {1.0F, 2.0F, 3.0F}});
    registry_.Emplace<Velocity>(moving_entity_,
                                Velocity{.value = {0.5F, 1.5F, -2.0F}});
    registry_.Emplace<Transform>(transform_only_entity_,
                                 Transform{.position = {10.0F, 20.0F, 30.0F}});
    registry_.Emplace<Velocity>(velocity_only_entity_,
                                Velocity{.value = {4.0F, 5.0F, 6.0F}});
  }

  engine::ecs::Registry registry_;
  MovementSystem movement_system_;
  engine::ecs::Entity moving_entity_;
  engine::ecs::Entity transform_only_entity_;
  engine::ecs::Entity velocity_only_entity_;
};

TEST_F(MovementSystemTest, UpdatesPositionUsingVelocityAndDeltaTime) {
  movement_system_.Update(registry_, 2.0F);

  ExpectVec3Near(registry_.GetComponent<Transform>(moving_entity_).position,
                 {2.0F, 5.0F, -1.0F});
}

TEST_F(MovementSystemTest, UpdatesEveryEntityWithTransformAndVelocity) {
  const engine::ecs::Entity second_moving_entity = registry_.Create();
  registry_.Emplace<Transform>(second_moving_entity,
                               Transform{.position = {-2.0F, 4.0F, 8.0F}});
  registry_.Emplace<Velocity>(second_moving_entity,
                              Velocity{.value = {3.0F, -2.0F, 1.0F}});

  movement_system_.Update(registry_, 0.5F);

  ExpectVec3Near(registry_.GetComponent<Transform>(moving_entity_).position,
                 {1.25F, 2.75F, 2.0F});
  ExpectVec3Near(registry_.GetComponent<Transform>(second_moving_entity).position,
                 {-0.5F, 3.0F, 8.5F});
}

TEST_F(MovementSystemTest, LeavesPositionUnchangedWhenVelocityIsMissing) {
  const glm::vec3 initial_position =
      registry_.GetComponent<Transform>(transform_only_entity_).position;

  movement_system_.Update(registry_, 1.0F);

  ExpectVec3Near(registry_.GetComponent<Transform>(transform_only_entity_).position,
                 initial_position);
}

TEST_F(MovementSystemTest, LeavesPositionUnchangedWhenDeltaTimeIsZero) {
  const glm::vec3 initial_position =
      registry_.GetComponent<Transform>(moving_entity_).position;

  movement_system_.Update(registry_, 0.0F);

  ExpectVec3Near(registry_.GetComponent<Transform>(moving_entity_).position,
                 initial_position);
}

TEST_F(MovementSystemTest, AccumulatesRepeatedFixedTimeSteps) {
  constexpr float kDeltaSeconds = 0.25F;
  constexpr int kUpdateCount = 4;

  for (int update = 0; update < kUpdateCount; ++update) {
    movement_system_.Update(registry_, kDeltaSeconds);
  }

  ExpectVec3Near(registry_.GetComponent<Transform>(moving_entity_).position,
                 {1.5F, 3.5F, 1.0F});
}

}  // namespace
