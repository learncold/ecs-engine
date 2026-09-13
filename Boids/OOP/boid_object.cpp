#include "Boids/OOP/boid_object.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace boids::oop {

constexpr float kMinVectorLengthSquared = 0.00000001F;

namespace {

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

}  // namespace

glm::vec3 BoidObject::CalculateAlignment(const glm::vec3& velocity_sum,
                                         std::size_t neighbor_count) const {
  const glm::vec3 average_velocity =
      velocity_sum / static_cast<float>(neighbor_count);
  if (glm::dot(average_velocity, average_velocity) <= kMinVectorLengthSquared) {
    return glm::vec3{0.0F};
  }

  const glm::vec3 desired_velocity =
      glm::normalize(average_velocity) * parameters_.preferred_speed;
  return LimitMagnitude(desired_velocity - velocity_,
                        parameters_.max_alignment_force);
}

void BoidObject::CalculateAcceleration(std::span<const BoidObject> boids) {
  glm::vec3 separation{0.0F};
  glm::vec3 velocity_sum{0.0F};
  glm::vec3 position_sum{0.0F};
  std::size_t neighbor_count = 0U;

  constexpr float kMinDistanceSquared = 0.00000001F;
  for (const auto& other : boids) {
    if (&other == this) {
      continue;
    }

    const glm::vec3 offset = other.transform_.position - transform_.position;
    const float distance_squared = glm::dot(offset, offset);
    if (distance_squared >=
        parameters_.neighbor_radius * parameters_.neighbor_radius) {
      continue;
    }

    ++neighbor_count;
    velocity_sum += other.velocity_;
    position_sum += other.transform_.position;

    if (distance_squared > kMinDistanceSquared &&
        distance_squared <
            parameters_.separation_radius * parameters_.separation_radius) {
      separation -= offset / distance_squared;
    }
  }

  if (neighbor_count == 0U) {
    acceleration_ = glm::vec3{0.0F};
    return;
  }

  const float count = static_cast<float>(neighbor_count);
  const glm::vec3 alignment = CalculateAlignment(velocity_sum, neighbor_count);
  const glm::vec3 cohesion = position_sum / count - transform_.position;
  acceleration_ = separation * parameters_.separation_weight +
                  alignment * parameters_.alignment_weight +
                  cohesion * parameters_.cohesion_weight;
}

void BoidObject::Move(float delta_seconds) {
  velocity_ += acceleration_ * delta_seconds;
  const float speed = glm::length(velocity_);
  if (parameters_.max_speed <= 0.0F) {
    velocity_ = glm::vec3{0.0F};
  } else if (speed > parameters_.max_speed) {
    velocity_ = glm::normalize(velocity_) * parameters_.max_speed;
  }
  transform_.position += velocity_ * delta_seconds;
}

void BoidObject::ApplyBoundary(float half_extent) {
  for (int axis = 0; axis < 3; ++axis) {
    if (transform_.position[axis] < -half_extent) {
      transform_.position[axis] = -half_extent;
      velocity_[axis] = std::abs(velocity_[axis]);
    } else if (transform_.position[axis] > half_extent) {
      transform_.position[axis] = half_extent;
      velocity_[axis] = -std::abs(velocity_[axis]);
    }
  }
}

void BoidObject::UpdateOrientation() {
  transform_.rotation = engine::renderer::RotationFromDirection(velocity_);
}

}  // namespace boids::oop
