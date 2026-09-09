#include "Boids/ECS/Systems/boundary_system.h"

#include <cmath>
#include <stdexcept>

#include "Boids/ECS/Components/velocity.h"
#include "Engine/ECS/registry.h"
#include "Engine/Renderer/transform.h"

namespace boids::ecs {

BoundarySystem::BoundarySystem(float half_extent) : half_extent_(half_extent) {
  if (half_extent_ <= 0.0F) {
    throw std::invalid_argument("Boundary half extent must be positive");
  }
}

void BoundarySystem::Update(engine::ecs::Registry& registry,
                            float /* delta_seconds */) {
  registry.CreateView<engine::renderer::Transform, Velocity>().Each(
      [this](engine::renderer::Transform& transform, Velocity& velocity) {
        for (int axis = 0; axis < 3; ++axis) {
          if (transform.position[axis] < -half_extent_) {
            transform.position[axis] = -half_extent_;
            velocity.value[axis] = std::abs(velocity.value[axis]);
          } else if (transform.position[axis] > half_extent_) {
            transform.position[axis] = half_extent_;
            velocity.value[axis] = -std::abs(velocity.value[axis]);
          }
        }
      });
}

}  // namespace boids::ecs
