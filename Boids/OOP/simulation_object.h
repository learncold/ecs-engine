#pragma once

#include <cstddef>

#include "Boids/Common/simulation_input.h"
#include "Engine/Renderer/transform.h"

namespace boids::oop {

class SimulationObject {
 public:
  virtual ~SimulationObject() = default;

  SimulationObject(const SimulationObject&) = delete;
  SimulationObject& operator=(const SimulationObject&) = delete;

  [[nodiscard]] boids::ObjectKind Kind() const { return kind_; }
  [[nodiscard]] std::size_t LogicalId() const { return logical_id_; }
  [[nodiscard]] const glm::vec3& Position() const {
    return transform_.position;
  }
  [[nodiscard]] const glm::vec3& Velocity() const { return velocity_; }
  [[nodiscard]] const engine::renderer::Transform& GetTransform() const {
    return transform_;
  }

 protected:
  SimulationObject(boids::ObjectKind kind, std::size_t logical_id,
                   const glm::vec3& position, const glm::vec3& velocity)
      : transform_{.position = position,
                   .rotation =
                       engine::renderer::RotationFromDirection(velocity),
                   .scale = 0.7F},
        velocity_(velocity),
        kind_(kind),
        logical_id_(logical_id) {}

  engine::renderer::Transform transform_;
  glm::vec3 velocity_;

 private:
  boids::ObjectKind kind_;
  std::size_t logical_id_;
};

}  // namespace boids::oop
