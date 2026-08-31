#include "Sandbox/Boids/ECS/Systems/boid_system.h"

#include <glm/glm.hpp>

#include "Sandbox/Boids/ECS/Components/acceleration.h"

namespace {

constexpr float kMinVectorLengthSquared = 0.00000001F;

glm::vec3 LimitMagnitude(const glm::vec3& vector, float max_magnitude) {
  if (max_magnitude <= 0.0F) {
    return glm::vec3{0.0F};
  }

  const float length_squared = glm::dot(vector, vector);
  const float max_length_squared = max_magnitude * max_magnitude;
  if (length_squared <= max_length_squared) {
    return vector;
  }

  return glm::normalize(vector) * max_magnitude;
}

glm::vec3 CalculateAlignment(const glm::vec3& velocity_sum,
                             std::size_t neighbor_count,
                             const Velocity& self_velocity,
                             const Boid& self_boid) {
  const glm::vec3 average_velocity =
      velocity_sum / static_cast<float>(neighbor_count);
  if (glm::dot(average_velocity, average_velocity) <=
      kMinVectorLengthSquared) {
    return glm::vec3{0.0F};
  }

  const glm::vec3 desired_velocity =
      glm::normalize(average_velocity) * self_boid.preferred_speed;
  const glm::vec3 steering = desired_velocity - self_velocity.value;
  return LimitMagnitude(steering, self_boid.max_force);
}

}  // namespace

void BoidSystem::Update(engine::ecs::Registry& registry,
                        float /* delta_seconds */) {
  registry.CreateView<Transform, Velocity, Boid, Acceleration>().Each(
      [&](engine::ecs::Entity entity, const Transform& transform,
          const Velocity& velocity, const Boid& boid,
          Acceleration& acceleration) {
        acceleration.value =
            CalculateAcceleration(registry, entity, transform, velocity, boid);
      });
}

glm::vec3 BoidSystem::CalculateAcceleration(engine::ecs::Registry& registry,
                                            engine::ecs::Entity self,
                                            const Transform& self_transform,
                                            const Velocity& self_velocity,
                                            const Boid& self_boid) const {
  glm::vec3 separation{0.0F};
  glm::vec3 velocity_sum{0.0F};
  glm::vec3 position_sum{0.0F};
  std::size_t neighbor_count = 0U;

  constexpr float kMinDistance = 0.0001F;
  constexpr float kMinDistanceSquared = kMinDistance * kMinDistance;

  registry.CreateView<Transform, Velocity, Boid>().Each(
      [&](engine::ecs::Entity other, const Transform& other_transform,
          const Velocity& other_velocity, const Boid&) {
        if (other == self) {
          return;
        }

        glm::vec3 offset = other_transform.position - self_transform.position;
        const float distance_squared = glm::dot(offset, offset);

        const float neighbor_radius_squared =
            self_boid.neighbor_radius * self_boid.neighbor_radius;

        if (distance_squared >= neighbor_radius_squared) return;

        ++neighbor_count;
        velocity_sum += other_velocity.value;
        position_sum += other_transform.position;

        if (distance_squared <= kMinDistanceSquared) {
          return;
        }

        const float separation_radius_squared =
            self_boid.separation_radius * self_boid.separation_radius;

        if (distance_squared < separation_radius_squared) {
          separation += -offset / distance_squared;
        }
      });

  if (neighbor_count == 0) return glm::vec3(0.0F);

  const float count = static_cast<float>(neighbor_count);

  const glm::vec3 alignment =
      CalculateAlignment(velocity_sum, neighbor_count, self_velocity, self_boid);
  const glm::vec3 cohesion = position_sum / count - self_transform.position;

  return separation * self_boid.separation_weight +
         alignment * self_boid.alignment_weight +
         cohesion * self_boid.cohesion_weight;
}
