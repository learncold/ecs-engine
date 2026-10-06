#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "Engine/ECS/entity.h"
#include "Engine/ECS/registry.h"

namespace {

struct TestComponent {
  int value = 0;
};

struct OtherComponent {
  int value = 0;
};

class RegistryTest : public testing::Test {
 protected:
  engine::ecs::Registry registry_;
};

class ReusedEntityRegistryTest : public RegistryTest {
 protected:
  void SetUp() override {
    stale_entity_ = registry_.Create();
    registry_.Destroy(stale_entity_);
    current_entity_ = registry_.Create();
  }

  engine::ecs::Entity stale_entity_;
  engine::ecs::Entity current_entity_;
};

TEST_F(RegistryTest, StartsWithNoLivingEntities) {
  EXPECT_EQ(registry_.EntityCount(), 0U);
}

TEST_F(RegistryTest, CreatesLivingEntitiesWithDistinctIds) {
  const auto first_entity = registry_.Create();
  const auto second_entity = registry_.Create();

  EXPECT_TRUE(registry_.IsAlive(first_entity));
  EXPECT_TRUE(registry_.IsAlive(second_entity));
  EXPECT_NE(first_entity.Value(), second_entity.Value());
  EXPECT_EQ(registry_.EntityCount(), 2U);
}

TEST_F(RegistryTest, EmplacesAndRetrievesComponent) {
  const auto entity = registry_.Create();

  TestComponent& component = registry_.Emplace<TestComponent>(entity, 10);

  EXPECT_EQ(component.value, 10);
  EXPECT_TRUE(registry_.Has<TestComponent>(entity));
  ASSERT_NE(registry_.FindComponent<TestComponent>(entity), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(entity)->value, 10);
}

TEST_F(RegistryTest, ProvidesConstComponentAccess) {
  const auto entity = registry_.Create();
  registry_.Emplace<TestComponent>(entity, 10);
  const engine::ecs::Registry& const_registry = registry_;

  ASSERT_NE(const_registry.FindComponent<TestComponent>(entity), nullptr);
  EXPECT_EQ(const_registry.FindComponent<TestComponent>(entity)->value, 10);
}

TEST_F(RegistryTest, ReplacesExistingComponentOfSameType) {
  const auto entity = registry_.Create();
  registry_.Emplace<TestComponent>(entity, 10);

  TestComponent& replacement = registry_.Emplace<TestComponent>(entity, 20);

  EXPECT_EQ(replacement.value, 20);
  ASSERT_NE(registry_.FindComponent<TestComponent>(entity), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(entity)->value, 20);
}

TEST_F(RegistryTest, StoresDifferentComponentTypesIndependently) {
  const auto entity = registry_.Create();
  registry_.Emplace<TestComponent>(entity, 10);
  registry_.Emplace<OtherComponent>(entity, 20);

  EXPECT_TRUE(registry_.Has<TestComponent>(entity));
  EXPECT_TRUE(registry_.Has<OtherComponent>(entity));
  ASSERT_NE(registry_.FindComponent<TestComponent>(entity), nullptr);
  ASSERT_NE(registry_.FindComponent<OtherComponent>(entity), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(entity)->value, 10);
  EXPECT_EQ(registry_.FindComponent<OtherComponent>(entity)->value, 20);
}

TEST_F(RegistryTest, KeepsComponentsSeparatedBetweenEntities) {
  const auto first_entity = registry_.Create();
  const auto second_entity = registry_.Create();
  registry_.Emplace<TestComponent>(first_entity, 10);
  registry_.Emplace<TestComponent>(second_entity, 20);

  ASSERT_NE(registry_.FindComponent<TestComponent>(first_entity), nullptr);
  ASSERT_NE(registry_.FindComponent<TestComponent>(second_entity), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(first_entity)->value, 10);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(second_entity)->value, 20);
}

TEST_F(RegistryTest, ReportsMissingComponentAsAbsent) {
  const auto entity = registry_.Create();
  const engine::ecs::Registry& const_registry = registry_;

  EXPECT_FALSE(registry_.Has<TestComponent>(entity));
  EXPECT_EQ(registry_.FindComponent<TestComponent>(entity), nullptr);
  EXPECT_EQ(const_registry.FindComponent<TestComponent>(entity), nullptr);
}

TEST_F(RegistryTest, ReportsExistingStorageAsMissingForUnassignedEntity) {
  const auto entity_with_component = registry_.Create();
  const auto entity_without_component = registry_.Create();
  registry_.Emplace<TestComponent>(entity_with_component, 10);
  const engine::ecs::Registry& const_registry = registry_;

  EXPECT_FALSE(registry_.Has<TestComponent>(entity_without_component));
  EXPECT_EQ(registry_.FindComponent<TestComponent>(entity_without_component),
            nullptr);
  EXPECT_EQ(
      const_registry.FindComponent<TestComponent>(entity_without_component),
      nullptr);
}

TEST_F(RegistryTest, RemovesComponentWithoutDestroyingEntity) {
  const auto entity = registry_.Create();
  registry_.Emplace<TestComponent>(entity, 10);
  registry_.Emplace<OtherComponent>(entity, 20);

  registry_.Remove<TestComponent>(entity);

  EXPECT_FALSE(registry_.Has<TestComponent>(entity));
  EXPECT_TRUE(registry_.Has<OtherComponent>(entity));
  EXPECT_TRUE(registry_.IsAlive(entity));
  EXPECT_EQ(registry_.EntityCount(), 1U);
}

TEST_F(RegistryTest, IgnoresRemovingMissingComponent) {
  const auto entity = registry_.Create();

  EXPECT_NO_THROW(registry_.Remove<TestComponent>(entity));

  registry_.Emplace<TestComponent>(entity, 10);
  registry_.Remove<TestComponent>(entity);

  EXPECT_NO_THROW(registry_.Remove<TestComponent>(entity));
  EXPECT_TRUE(registry_.IsAlive(entity));
}

TEST_F(RegistryTest, RejectsEmplaceForInvalidOrDestroyedEntity) {
  const engine::ecs::Entity invalid_entity;
  const auto destroyed_entity = registry_.Create();
  registry_.Destroy(destroyed_entity);

  EXPECT_THROW(registry_.Emplace<TestComponent>(invalid_entity, 10),
               std::invalid_argument);
  EXPECT_THROW(registry_.Emplace<TestComponent>(destroyed_entity, 20),
               std::invalid_argument);
}

TEST_F(RegistryTest, IgnoresDestroyingInvalidOrAlreadyDestroyedEntity) {
  const engine::ecs::Entity invalid_entity;
  const auto entity = registry_.Create();

  registry_.Destroy(invalid_entity);
  EXPECT_EQ(registry_.EntityCount(), 1U);

  registry_.Destroy(entity);
  registry_.Destroy(entity);

  EXPECT_EQ(registry_.EntityCount(), 0U);
}

TEST_F(RegistryTest, DestroysEntityAndRemovesItsComponents) {
  const auto entity = registry_.Create();
  registry_.Emplace<TestComponent>(entity, 10);
  registry_.Emplace<OtherComponent>(entity, 20);

  registry_.Destroy(entity);

  EXPECT_FALSE(registry_.IsAlive(entity));
  EXPECT_FALSE(registry_.Has<TestComponent>(entity));
  EXPECT_FALSE(registry_.Has<OtherComponent>(entity));
  EXPECT_EQ(registry_.EntityCount(), 0U);
}

TEST_F(ReusedEntityRegistryTest,
       ReusesDestroyedEntityIdWithDifferentGeneration) {
  EXPECT_EQ(stale_entity_.Value(), current_entity_.Value());
  EXPECT_NE(stale_entity_.Generation(), current_entity_.Generation());
}

TEST_F(ReusedEntityRegistryTest,
       DistinguishesStaleAndReusedEntitiesByGeneration) {
  EXPECT_FALSE(registry_.IsAlive(stale_entity_));
  EXPECT_TRUE(registry_.IsAlive(current_entity_));
}

TEST_F(ReusedEntityRegistryTest, RejectsEmplaceWithStaleEntity) {
  EXPECT_THROW(registry_.Emplace<TestComponent>(stale_entity_, 10),
               std::invalid_argument);
  EXPECT_NO_THROW(registry_.Emplace<TestComponent>(current_entity_, 20));
  ASSERT_NE(registry_.FindComponent<TestComponent>(current_entity_), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(current_entity_)->value, 20);
}

TEST_F(ReusedEntityRegistryTest,
       DoesNotReportNewEntityComponentForStaleEntity) {
  registry_.Emplace<TestComponent>(current_entity_, 10);

  EXPECT_FALSE(registry_.Has<TestComponent>(stale_entity_));
  EXPECT_TRUE(registry_.Has<TestComponent>(current_entity_));
}

TEST_F(ReusedEntityRegistryTest, ReturnsNullFromFindComponentForStaleEntity) {
  registry_.Emplace<TestComponent>(current_entity_, 10);
  const engine::ecs::Registry& const_registry = registry_;

  EXPECT_EQ(registry_.FindComponent<TestComponent>(stale_entity_), nullptr);
  EXPECT_EQ(const_registry.FindComponent<TestComponent>(stale_entity_), nullptr);
  ASSERT_NE(registry_.FindComponent<TestComponent>(current_entity_), nullptr);
  ASSERT_NE(const_registry.FindComponent<TestComponent>(current_entity_), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(current_entity_)->value, 10);
  EXPECT_EQ(const_registry.FindComponent<TestComponent>(current_entity_)->value,
            10);
}

TEST_F(ReusedEntityRegistryTest,
       DoesNotRemoveNewEntityComponentWithStaleEntity) {
  registry_.Emplace<TestComponent>(current_entity_, 10);

  registry_.Remove<TestComponent>(stale_entity_);

  EXPECT_TRUE(registry_.Has<TestComponent>(current_entity_));
  ASSERT_NE(registry_.FindComponent<TestComponent>(current_entity_), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(current_entity_)->value, 10);
}

TEST_F(ReusedEntityRegistryTest, DoesNotDestroyReusedEntityWithStaleEntity) {
  registry_.Emplace<TestComponent>(current_entity_, 10);

  registry_.Destroy(stale_entity_);

  EXPECT_TRUE(registry_.IsAlive(current_entity_));
  EXPECT_TRUE(registry_.Has<TestComponent>(current_entity_));
  EXPECT_EQ(registry_.EntityCount(), 1U);
}

TEST_F(RegistryTest, MaintainsEntityCountAcrossCreationDestructionAndReuse) {
  const auto first_entity = registry_.Create();
  const auto second_entity = registry_.Create();
  const auto third_entity = registry_.Create();
  EXPECT_EQ(registry_.EntityCount(), 3U);

  registry_.Destroy(second_entity);
  EXPECT_EQ(registry_.EntityCount(), 2U);

  registry_.Destroy(second_entity);
  EXPECT_EQ(registry_.EntityCount(), 2U);

  const auto reused_entity = registry_.Create();
  EXPECT_EQ(reused_entity.Value(), second_entity.Value());
  EXPECT_EQ(registry_.EntityCount(), 3U);

  registry_.Destroy(first_entity);
  registry_.Destroy(third_entity);
  registry_.Destroy(reused_entity);
  EXPECT_EQ(registry_.EntityCount(), 0U);
}

TEST_F(RegistryTest, NeverRevivesStaleHandlesAcrossRepeatedReuse) {
  std::vector<engine::ecs::Entity> stale_entities;

  for (int iteration = 0; iteration < 5; ++iteration) {
    const auto entity = registry_.Create();

    for (const auto stale_entity : stale_entities) {
      EXPECT_FALSE(registry_.IsAlive(stale_entity));
      EXPECT_NE(stale_entity.Generation(), entity.Generation());
    }

    registry_.Destroy(entity);
    stale_entities.push_back(entity);
  }

  const auto current_entity = registry_.Create();
  for (const auto stale_entity : stale_entities) {
    EXPECT_FALSE(registry_.IsAlive(stale_entity));
    EXPECT_NE(stale_entity.Generation(), current_entity.Generation());
  }
  EXPECT_TRUE(registry_.IsAlive(current_entity));
}

TEST_F(RegistryTest, ClearInvalidatesHandlesAndRemovesComponents) {
  const auto old_entity = registry_.Create();
  registry_.Emplace<TestComponent>(old_entity, 10);

  registry_.Clear();

  EXPECT_FALSE(registry_.IsAlive(old_entity));
  EXPECT_EQ(registry_.EntityCount(), 0U);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(old_entity), nullptr);

  const auto new_entity = registry_.Create();
  EXPECT_EQ(old_entity.Value(), new_entity.Value());
  EXPECT_NE(old_entity.Generation(), new_entity.Generation());
  EXPECT_TRUE(registry_.IsAlive(new_entity));
  EXPECT_FALSE(registry_.Has<TestComponent>(new_entity));
  EXPECT_EQ(registry_.EntityCount(), 1U);
}

TEST_F(RegistryTest, ClearRemovesAllEntitiesAndComponentTypes) {
  const auto first_entity = registry_.Create();
  const auto second_entity = registry_.Create();
  registry_.Emplace<TestComponent>(first_entity, 10);
  registry_.Emplace<OtherComponent>(second_entity, 20);

  registry_.Clear();

  EXPECT_EQ(registry_.EntityCount(), 0U);
  EXPECT_FALSE(registry_.IsAlive(first_entity));
  EXPECT_FALSE(registry_.IsAlive(second_entity));
  EXPECT_FALSE(registry_.Has<TestComponent>(first_entity));
  EXPECT_FALSE(registry_.Has<OtherComponent>(second_entity));
}

TEST_F(RegistryTest, NeverRevivesStaleHandlesAcrossRepeatedClearAndRecreation) {
  std::vector<engine::ecs::Entity> stale_entities;
  std::vector<engine::ecs::Entity> current_entities;
  const engine::ecs::Registry& const_registry = registry_;

  // Include partial recreation followed by growth to exercise retained IDs.
  for (const int entity_count : {2, 2, 1, 4, 2, 4}) {
    SCOPED_TRACE(entity_count);
    stale_entities.insert(stale_entities.end(), current_entities.begin(),
                          current_entities.end());
    registry_.Clear();
    current_entities.clear();
    EXPECT_EQ(registry_.EntityCount(), 0U);

    for (const auto stale_entity : stale_entities) {
      EXPECT_FALSE(registry_.IsAlive(stale_entity));
      EXPECT_EQ(registry_.FindComponent<TestComponent>(stale_entity), nullptr);
    }

    for (int index = 0; index < entity_count; ++index) {
      const auto entity = registry_.Create();
      registry_.Emplace<TestComponent>(entity, index + 10);
      current_entities.push_back(entity);
    }

    for (const auto stale_entity : stale_entities) {
      SCOPED_TRACE(testing::Message() << "Stale ID: " << stale_entity.Value()
                                     << ", generation: "
                                     << stale_entity.Generation());
      EXPECT_FALSE(registry_.IsAlive(stale_entity));
      EXPECT_FALSE(registry_.Has<TestComponent>(stale_entity));
      EXPECT_EQ(registry_.FindComponent<TestComponent>(stale_entity), nullptr);
      EXPECT_EQ(const_registry.FindComponent<TestComponent>(stale_entity),
                nullptr);
      EXPECT_THROW(registry_.Emplace<TestComponent>(stale_entity, -1),
                   std::invalid_argument);
      registry_.Remove<TestComponent>(stale_entity);
      registry_.Destroy(stale_entity);
    }

    EXPECT_EQ(registry_.EntityCount(), current_entities.size());
    for (int index = 0; index < entity_count; ++index) {
      const auto entity = current_entities[index];
      EXPECT_TRUE(registry_.IsAlive(entity));
      ASSERT_NE(registry_.FindComponent<TestComponent>(entity), nullptr);
      EXPECT_EQ(registry_.FindComponent<TestComponent>(entity)->value,
                index + 10);
    }
  }
}

TEST_F(RegistryTest, PreservesDestroyAndIdReuseAfterClear) {
  const auto old_first = registry_.Create();
  const auto old_second = registry_.Create();
  registry_.Clear();
  const auto first = registry_.Create();
  const auto second = registry_.Create();

  registry_.Destroy(second);
  const auto reused = registry_.Create();
  registry_.Emplace<TestComponent>(reused, 42);

  EXPECT_EQ(reused.Value(), second.Value());
  EXPECT_NE(reused.Generation(), second.Generation());
  for (const auto stale_entity : {old_first, old_second, second}) {
    EXPECT_FALSE(registry_.IsAlive(stale_entity));
    EXPECT_EQ(registry_.FindComponent<TestComponent>(stale_entity), nullptr);
    registry_.Destroy(stale_entity);
  }
  EXPECT_TRUE(registry_.IsAlive(first));
  EXPECT_TRUE(registry_.IsAlive(reused));
  EXPECT_EQ(registry_.EntityCount(), 2U);
  ASSERT_NE(registry_.FindComponent<TestComponent>(reused), nullptr);
  EXPECT_EQ(registry_.FindComponent<TestComponent>(reused)->value, 42);
}

}  // namespace
