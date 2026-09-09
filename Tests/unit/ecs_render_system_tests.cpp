#include <gtest/gtest.h>

#include "Engine/ECS/registry.h"
#include "Engine/ECSRenderer/ecs_render_system.h"
#include "Engine/Renderer/mesh_renderer.h"
#include "Engine/Renderer/transform.h"

namespace {

TEST(EcsRenderSystemTest, ExtractsOnlyEntitiesWithBothComponents) {
  engine::ecs::Registry registry;
  const engine::ecs::Entity complete = registry.Create();
  const engine::ecs::Entity transform_only = registry.Create();
  const engine::ecs::Entity mesh_only = registry.Create();
  const engine::renderer::Transform expected{
      .position = {1.0F, 2.0F, 3.0F},
      .rotation = {0.5F, 0.5F, -0.5F, 0.5F},
      .scale = 0.7F};
  registry.Emplace<engine::renderer::Transform>(complete, expected);
  registry.Emplace<engine::renderer::MeshRenderer>(complete);
  registry.Emplace<engine::renderer::Transform>(transform_only);
  registry.Emplace<engine::renderer::MeshRenderer>(mesh_only);

  engine::ecs_renderer::EcsRenderSystem system;
  system.Extract(registry);

  ASSERT_EQ(system.Instances().size(), 1U);
  const auto& instance = system.Instances().front();
  EXPECT_EQ(instance.mesh, engine::renderer::MeshKind::Agent);
  EXPECT_EQ(instance.transform.position, expected.position);
  EXPECT_EQ(instance.transform.rotation, expected.rotation);
  EXPECT_FLOAT_EQ(instance.transform.scale, expected.scale);
}

TEST(EcsRenderSystemTest, ClearsPreviousFrameBeforeExtraction) {
  engine::ecs::Registry registry;
  const engine::ecs::Entity entity = registry.Create();
  registry.Emplace<engine::renderer::Transform>(entity);
  registry.Emplace<engine::renderer::MeshRenderer>(entity);
  engine::ecs_renderer::EcsRenderSystem system;
  system.Extract(registry);
  ASSERT_EQ(system.Instances().size(), 1U);

  registry.Remove<engine::renderer::MeshRenderer>(entity);
  system.Extract(registry);

  EXPECT_TRUE(system.Instances().empty());
}

}  // namespace
