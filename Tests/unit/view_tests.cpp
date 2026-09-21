#include <gtest/gtest.h>

#include "Engine/ECS/registry.h"

namespace {

struct Transform {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

struct Velocity {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

struct BoidTag {};

class ViewTest : public testing::Test {
 protected:
  void SetUp() override {
    moving_entity_ = registry_.Create();
    transform_only_entity_ = registry_.Create();
    velocity_only_entity_ = registry_.Create();

    registry_.Emplace<Transform>(moving_entity_, 1.0F, 2.0F, 3.0F);
    registry_.Emplace<Velocity>(moving_entity_, 0.5F, 1.5F, 2.5F);
    registry_.Emplace<Transform>(transform_only_entity_, 10.0F, 20.0F, 30.0F);
    registry_.Emplace<Velocity>(velocity_only_entity_, 100.0F, 200.0F, 300.0F);
  }

  engine::ecs::Registry registry_;
  engine::ecs::Entity moving_entity_;
  engine::ecs::Entity transform_only_entity_;
  engine::ecs::Entity velocity_only_entity_;
};

TEST_F(ViewTest, InvokesEntityCallbackForMatchingEntity) {
  int callback_count = 0;

  registry_.CreateView<Transform, Velocity>().Each(
      [&](engine::ecs::Entity entity, Transform& transform,
          Velocity& velocity) {
        EXPECT_EQ(entity, moving_entity_);
        transform.x += velocity.x;
        transform.y += velocity.y;
        transform.z += velocity.z;
        ++callback_count;
      });

  EXPECT_EQ(callback_count, 1);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Transform>(moving_entity_)->x, 1.5F);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Transform>(moving_entity_)->y, 3.5F);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Transform>(moving_entity_)->z, 5.5F);
}

TEST_F(ViewTest, InvokesComponentCallbackForMatchingEntity) {
  int callback_count = 0;

  registry_.CreateView<Transform, Velocity>().Each(
      [&](Transform& transform, Velocity& velocity) {
        velocity.x += transform.x;
        velocity.y += transform.y;
        velocity.z += transform.z;
        ++callback_count;
      });

  EXPECT_EQ(callback_count, 1);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Velocity>(moving_entity_)->x, 1.5F);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Velocity>(moving_entity_)->y, 3.5F);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Velocity>(moving_entity_)->z, 5.5F);
}

TEST_F(ViewTest, InvokesCallbackForEveryEntityInSingleComponentView) {
  int callback_count = 0;

  registry_.CreateView<Transform>().Each(
      [&](engine::ecs::Entity, Transform& transform) {
        transform.x += 1.0F;
        ++callback_count;
      });

  EXPECT_EQ(callback_count, 2);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Transform>(moving_entity_)->x, 2.0F);
  EXPECT_FLOAT_EQ(
      registry_.FindComponent<Transform>(transform_only_entity_)->x, 11.0F);
}

TEST_F(ViewTest, SkipsCallbackWhenRequiredStorageIsMissing) {
  int callback_count = 0;

  registry_.CreateView<Transform, BoidTag>().Each(
      [&](engine::ecs::Entity, Transform&, BoidTag&) { ++callback_count; });

  EXPECT_EQ(callback_count, 0);
}

}  // namespace
