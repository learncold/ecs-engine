#pragma once

#include <glm/vec3.hpp>

#include "Boids/Common/boid_parameters.h"
#include "Boids/ECS/Components/velocity.h"
#include "Engine/ECS/entity.h"
#include "Engine/ECS/system.h"
#include "Engine/Renderer/transform.h"

namespace boids::ecs {

class BoidSystem final : public engine::ecs::System {
 public:
  void Update(engine::ecs::Registry& registry, float delta_seconds) override;

 private:
  using NeighborView =
      engine::ecs::View<engine::renderer::Transform, Velocity>;

  [[nodiscard]] glm::vec3 CalculateAcceleration(
      NeighborView& neighbor_view, engine::ecs::Entity self,
      const engine::renderer::Transform& self_transform,
      const Velocity& self_velocity,
      const BoidParameters& self_parameters) const;
};

}  // namespace boids::ecs
