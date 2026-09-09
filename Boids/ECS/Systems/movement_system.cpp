#include "Boids/ECS/Systems/movement_system.h"

#include <glm/geometric.hpp>

#include "Boids/Common/boid_parameters.h"
#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Engine/ECS/registry.h"
#include "Engine/Renderer/transform.h"

namespace boids::ecs {

void MovementSystem::Update(engine::ecs::Registry& registry,
                            float delta_seconds) {
  registry.CreateView<Velocity, Acceleration>().Each(
      [delta_seconds](Velocity& velocity, const Acceleration& acceleration) {
        velocity.value += acceleration.value * delta_seconds;
      });

  registry.CreateView<Velocity, BoidParameters>().Each(
      [](Velocity& velocity, const BoidParameters& parameters) {
        const float speed = glm::length(velocity.value);
        if (parameters.max_speed <= 0.0F) {
          velocity.value = glm::vec3{0.0F};
        } else if (speed > parameters.max_speed) {
          velocity.value =
              glm::normalize(velocity.value) * parameters.max_speed;
        }
      });

  registry.CreateView<engine::renderer::Transform, Velocity>().Each(
      [delta_seconds](engine::renderer::Transform& transform,
                      const Velocity& velocity) {
        transform.position += velocity.value * delta_seconds;
      });
}

}  // namespace boids::ecs
