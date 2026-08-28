#include "Sandbox/Boids/ECS/Systems/movement_system.h"

#include "Sandbox/Boids/ECS/Components/transform.h"
#include "Sandbox/Boids/ECS/Components/velocity.h"


void MovementSystem::Update(engine::ecs::Registry& registry,
                            float delta_seconds) {
  registry.CreateView<Transform, Velocity>().Each(
      [delta_seconds](Transform& transform, const Velocity& velocity) {
        transform.position += velocity.value * delta_seconds;
      });
}