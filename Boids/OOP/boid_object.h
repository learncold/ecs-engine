#pragma once

#include <span>

#include "Boids/Common/boid_parameters.h"
#include "Engine/Renderer/mesh_renderer.h"
#include "Engine/Renderer/transform.h"

namespace boids::oop {

class BoidObject {
 public:
  BoidObject(const glm::vec3& position, const glm::vec3& velocity,
             const boids::BoidParameters& parameters)
      : transform_{
            .position = position,
            .rotation = engine::renderer::RotationFromDirection(velocity),
            .scale = 0.7F},
        mesh_renderer_{},
        velocity_(velocity),
        acceleration_(0.0F),
        parameters_(parameters) {}
  void CalculateAcceleration(std::span<const BoidObject> boids);
  void Move(float delta_seconds);
  void ApplyBoundary(float boundary_half_extent);
  void UpdateOrientation();

  [[nodiscard]] const glm::vec3& Position() const {
    return transform_.position;
  }
  [[nodiscard]] const glm::vec3& Velocity() const { return velocity_; }
  [[nodiscard]] const glm::vec3& Acceleration() const { return acceleration_; }
  [[nodiscard]] const engine::renderer::Transform& GetTransform() const {
    return transform_;
  }

 private:
  glm::vec3 CalculateAlignment(const glm::vec3& velocity_sum,
                               std::size_t neighbor_count) const;
  engine::renderer::Transform transform_;
  engine::renderer::MeshRenderer mesh_renderer_;
  glm::vec3 velocity_;
  glm::vec3 acceleration_{0.0F};
  boids::BoidParameters parameters_;
};

}  // namespace boids::oop
