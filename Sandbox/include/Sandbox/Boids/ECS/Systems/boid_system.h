#pragma once

#include <glm/vec3.hpp>

#include "Engine/ECS/entity.h"
#include "Engine/ECS/system.h"
#include "Sandbox/Boids/ECS/Components/boid.h"
#include "Sandbox/Boids/ECS/Components/transform.h"
#include "Sandbox/Boids/ECS/Components/velocity.h"

class BoidSystem final : public engine::ecs::System {
 public:
  void Update(engine::ecs::Registry& registry, float delta_seconds) override;

 private:
  [[nodiscard]] glm::vec3 CalculateAcceleration(engine::ecs::Registry& registry,
                                                engine::ecs::Entity self,
                                                const Transform& self_transform,
                                                const Velocity& self_velocity,
                                                const Boid& self_boid) const;
};