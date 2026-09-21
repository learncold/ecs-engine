#include "Boids/ECS/Systems/boid_system.h"

#include <cstddef>
#include <glm/glm.hpp>

#include "Boids/ECS/Components/acceleration.h"

namespace boids::ecs {

namespace {

constexpr float kMinVectorLengthSquared = 0.00000001F;

glm::vec3 LimitMagnitude(const glm::vec3& vector, float max_magnitude) {
  if (max_magnitude <= 0.0F) {
    return glm::vec3{0.0F};
  }

  const float length_squared = glm::dot(vector, vector);
  if (length_squared <= max_magnitude * max_magnitude) {
    return vector;
  }
  return glm::normalize(vector) * max_magnitude;
}

glm::vec3 CalculateAlignment(const glm::vec3& velocity_sum,
                             std::size_t neighbor_count,
                             const Velocity& self_velocity,
                             const BoidParameters& self_parameters) {
  const glm::vec3 average_velocity =
      velocity_sum / static_cast<float>(neighbor_count);
  if (glm::dot(average_velocity, average_velocity) <= kMinVectorLengthSquared) {
    return glm::vec3{0.0F};
  }

  const glm::vec3 desired_velocity =
      glm::normalize(average_velocity) * self_parameters.preferred_speed;
  return LimitMagnitude(desired_velocity - self_velocity.value,
                        self_parameters.max_alignment_force);
}

}  // namespace

void BoidSystem::Update(engine::ecs::Registry& registry,
                        float /* delta_seconds */) {
  registry
      .CreateView<engine::renderer::Transform, Velocity, BoidParameters,
                  Acceleration>()
      .Each([&](engine::ecs::Entity entity,
                const engine::renderer::Transform& transform,
                const Velocity& velocity, const BoidParameters& parameters,
                Acceleration& acceleration) {
        acceleration.value = CalculateAcceleration(registry, entity, transform,
                                                   velocity, parameters);
      });
}

glm::vec3 BoidSystem::CalculateAcceleration(
    engine::ecs::Registry& registry, engine::ecs::Entity self,
    const engine::renderer::Transform& self_transform,
    const Velocity& self_velocity,
    const BoidParameters& self_parameters) const {
  glm::vec3 separation{0.0F};
  glm::vec3 velocity_sum{0.0F};
  glm::vec3 position_sum{0.0F};
  std::size_t neighbor_count = 0U;

  constexpr float kMinDistanceSquared = 0.00000001F;
  registry.CreateView<engine::renderer::Transform, Velocity>().Each(
      [&](engine::ecs::Entity other,
          const engine::renderer::Transform& other_transform,
          const Velocity& other_velocity) {
        if (other == self) {
          return;
        }

        const glm::vec3 offset =
            other_transform.position - self_transform.position;
        const float distance_squared = glm::dot(offset, offset);
        if (distance_squared >=
            self_parameters.neighbor_radius * self_parameters.neighbor_radius) {
          return;
        }

        ++neighbor_count;
        velocity_sum += other_velocity.value;
        position_sum += other_transform.position;

        if (distance_squared > kMinDistanceSquared &&
            distance_squared < self_parameters.separation_radius *
                                   self_parameters.separation_radius) {
          separation -= offset / distance_squared;
        }
      });

  if (neighbor_count == 0U) {
    return glm::vec3{0.0F};
  }

  const float count = static_cast<float>(neighbor_count);
  const glm::vec3 alignment = CalculateAlignment(
      velocity_sum, neighbor_count, self_velocity, self_parameters);
  const glm::vec3 cohesion = position_sum / count - self_transform.position;
  return separation * self_parameters.separation_weight +
         alignment * self_parameters.alignment_weight +
         cohesion * self_parameters.cohesion_weight;
}

}  // namespace boids::ecs
