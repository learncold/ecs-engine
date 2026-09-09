#include <gtest/gtest.h>

#include <glm/geometric.hpp>

#include "Boids/Common/boid_parameters.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/Systems/boid_orientation_system.h"
#include "Boids/ECS/Systems/boundary_system.h"
#include "Engine/ECS/registry.h"
#include "Engine/Renderer/transform.h"

namespace {

TEST(BoidOrientationSystemTest, UsesVelocityAfterBoundaryReflection) {
  engine::ecs::Registry registry;
  const engine::ecs::Entity entity = registry.Create();
  registry.Emplace<engine::renderer::Transform>(
      entity, engine::renderer::Transform{.position = {3.0F, 0.0F, 0.0F}});
  registry.Emplace<boids::ecs::Velocity>(
      entity, boids::ecs::Velocity{.value = {2.0F, 0.0F, 0.0F}});
  registry.Emplace<boids::BoidParameters>(
      entity, boids::BoidParameters{.neighbor_radius = 1.0F,
                                    .separation_radius = 1.0F,
                                    .separation_weight = 0.0F,
                                    .alignment_weight = 0.0F,
                                    .cohesion_weight = 0.0F,
                                    .preferred_speed = 1.0F,
                                    .max_speed = 2.0F,
                                    .max_alignment_force = 1.0F});
  boids::ecs::BoundarySystem boundary(2.0F);
  boids::ecs::BoidOrientationSystem orientation;

  boundary.Update(registry, 0.0F);
  orientation.Update(registry, 0.0F);

  const auto& transform = registry.Get<engine::renderer::Transform>(entity);
  const glm::vec3 forward = transform.rotation * glm::vec3{0.0F, 0.0F, 1.0F};
  EXPECT_NEAR(forward.x, -1.0F, 0.00001F);
  EXPECT_NEAR(forward.y, 0.0F, 0.00001F);
  EXPECT_NEAR(forward.z, 0.0F, 0.00001F);
}

}  // namespace
