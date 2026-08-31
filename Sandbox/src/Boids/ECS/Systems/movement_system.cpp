#include "Sandbox/Boids/ECS/Systems/movement_system.h"

#include <glm/geometric.hpp>

#include "Engine/ECS/registry.h"
#include "Sandbox/Boids/ECS/Components/acceleration.h"
#include "Sandbox/Boids/ECS/Components/boid.h"
#include "Sandbox/Boids/ECS/Components/transform.h"
#include "Sandbox/Boids/ECS/Components/velocity.h"
void MovementSystem::Update(engine::ecs::Registry& registry,
                            float delta_seconds) {
  registry.CreateView<Velocity, Acceleration>().Each(
      [delta_seconds](Velocity& velocity, const Acceleration& acceleration) {
        velocity.value += acceleration.value * delta_seconds;
      });

  registry.CreateView<Velocity, Boid>().Each(
      [](Velocity& velocity, const Boid& boid) {
        const float speed = glm::length(velocity.value);

        if (boid.max_speed <= 0.0F) {
          velocity.value = glm::vec3{0.0F};
          return;
        }

        if (speed > boid.max_speed) {
          velocity.value = glm::normalize(velocity.value) * boid.max_speed;
        }
      });

  registry.CreateView<Transform, Velocity>().Each(
      [delta_seconds](Transform& transform, const Velocity& velocity) {
        transform.position += velocity.value * delta_seconds;
      });
}