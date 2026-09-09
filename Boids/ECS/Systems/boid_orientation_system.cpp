#include "Boids/ECS/Systems/boid_orientation_system.h"

#include "Boids/Common/boid_parameters.h"
#include "Boids/ECS/Components/velocity.h"
#include "Engine/ECS/registry.h"
#include "Engine/Renderer/transform.h"

namespace boids::ecs {

void BoidOrientationSystem::Update(engine::ecs::Registry& registry,
                                   float /* delta_seconds */) {
  registry.CreateView<engine::renderer::Transform, Velocity, BoidParameters>()
      .Each([](engine::renderer::Transform& transform, const Velocity& velocity,
               const BoidParameters&) {
        transform.rotation =
            engine::renderer::RotationFromDirection(velocity.value);
      });
}

}  // namespace boids::ecs
