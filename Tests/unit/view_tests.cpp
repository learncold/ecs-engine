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

TEST_F(ViewTest, CachedViewReusesDenseIndicesAndReadsCurrentValues) {
  auto view = registry_.CreateCachedView<Transform, Velocity>();
  ASSERT_EQ(view.Size(), 1U);

  int callback_count = 0;
  view.Each([&](engine::ecs::Entity entity, Transform& transform,
                Velocity& velocity) {
    EXPECT_EQ(entity, moving_entity_);
    transform.x += velocity.x;
    ++callback_count;
  });

  registry_.Emplace<Velocity>(moving_entity_, Velocity{2.0F, 1.5F, 2.5F});
  EXPECT_TRUE(view.IsCurrent(registry_));
  view.Each([&](Transform& transform, const Velocity& velocity) {
    transform.x += velocity.x;
    ++callback_count;
  });

  EXPECT_EQ(callback_count, 2);
  EXPECT_FLOAT_EQ(registry_.FindComponent<Transform>(moving_entity_)->x, 3.5F);
}

TEST_F(ViewTest, CachedViewDetectsStructuralChanges) {
  auto view = registry_.CreateCachedView<Transform, Velocity>();
  const auto added_entity = registry_.Create();
  registry_.Emplace<Transform>(added_entity, Transform{});
  registry_.Emplace<Velocity>(added_entity, Velocity{});

  EXPECT_FALSE(view.IsCurrent(registry_));
  EXPECT_THROW(view.Each([](Transform&, Velocity&) {}), std::logic_error);

  view.Refresh(registry_);
  EXPECT_TRUE(view.IsCurrent(registry_));
  EXPECT_EQ(view.Size(), 2U);
}

TEST_F(ViewTest, CachedViewDetectsCreationOfMissingStorage) {
  auto view = registry_.CreateCachedView<Transform, BoidTag>();
  EXPECT_TRUE(view.IsCurrent(registry_));
  EXPECT_EQ(view.Size(), 0U);

  registry_.Emplace<BoidTag>(moving_entity_);

  EXPECT_FALSE(view.IsCurrent(registry_));
  view.Refresh(registry_);
  EXPECT_TRUE(view.IsCurrent(registry_));
  EXPECT_EQ(view.Size(), 1U);
}

TEST_F(ViewTest, CachedViewRefreshesIndicesAfterSwapRemove) {
  const auto second_moving_entity = registry_.Create();
  registry_.Emplace<Transform>(second_moving_entity,
                               Transform{20.0F, 30.0F, 40.0F});
  registry_.Emplace<Velocity>(second_moving_entity, Velocity{2.0F, 3.0F, 4.0F});
  auto view = registry_.CreateCachedView<Transform, Velocity>();
  ASSERT_EQ(view.Size(), 2U);

  registry_.Destroy(moving_entity_);
  EXPECT_FALSE(view.IsCurrent(registry_));

  view.Refresh(registry_);
  ASSERT_EQ(view.Size(), 1U);
  view.Each([&](engine::ecs::Entity entity, Transform& transform,
                Velocity& velocity) {
    EXPECT_EQ(entity, second_moving_entity);
    EXPECT_FLOAT_EQ(transform.x, 20.0F);
    EXPECT_FLOAT_EQ(velocity.x, 2.0F);
  });
}

TEST_F(ViewTest, CachedViewDetectsRegistryClear) {
  auto view = registry_.CreateCachedView<Transform, Velocity>();

  registry_.Clear();

  EXPECT_FALSE(view.IsCurrent(registry_));
  EXPECT_THROW(view.Each([](Transform&, Velocity&) {}), std::logic_error);

  const auto new_entity = registry_.Create();
  registry_.Emplace<Transform>(new_entity, Transform{4.0F, 5.0F, 6.0F});
  registry_.Emplace<Velocity>(new_entity, Velocity{7.0F, 8.0F, 9.0F});
  const auto second_new_entity = registry_.Create();
  registry_.Emplace<Transform>(second_new_entity, Transform{});
  registry_.Emplace<Velocity>(second_new_entity, Velocity{});
  EXPECT_FALSE(view.IsCurrent(registry_));
  EXPECT_THROW(view.Each([](Transform&, Velocity&) {}), std::logic_error);
  view.Refresh(registry_);

  EXPECT_TRUE(view.IsCurrent(registry_));
  ASSERT_EQ(view.Size(), 2U);
  int callback_count = 0;
  view.Each([&](engine::ecs::Entity entity, Transform&, Velocity&) {
    EXPECT_TRUE(entity == new_entity || entity == second_new_entity);
    EXPECT_TRUE(registry_.IsAlive(entity));
    ++callback_count;
  });
  EXPECT_EQ(callback_count, 2);
}

}  // namespace
