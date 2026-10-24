#include "Boids/ECS/Systems/movement_system.h"

#include "Boids/Common/boid_parameters.h"
#include "Boids/Common/vector_math.h"
#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Engine/ECS/registry.h"
#include "Engine/Renderer/transform.h"

namespace boids::ecs {

void MovementSystem::Update(engine::ecs::Registry& registry,
                            float delta_seconds,
                            engine::measurement::TimingRecorder& /* timing */) {
  registry.CreateView<BoidParameters, Velocity, Acceleration>().Each(
      [delta_seconds](const BoidParameters&, Velocity& velocity,
                      const Acceleration& acceleration) {
        velocity.value += acceleration.value * delta_seconds;
      });

  registry.CreateView<Velocity, BoidParameters>().Each(
      [](Velocity& velocity, const BoidParameters& parameters) {
        velocity.value =
            boids::LimitSpeed(velocity.value, parameters.max_speed);
      });

  registry.CreateView<BoidParameters, engine::renderer::Transform, Velocity>()
      .Each([delta_seconds](const BoidParameters&,
                            engine::renderer::Transform& transform,
                            const Velocity& velocity) {
        transform.position = boids::RequireFinite(
            transform.position + velocity.value * delta_seconds);
      });
}

}  // namespace boids::ecs
