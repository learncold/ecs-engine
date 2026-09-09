#pragma once

#include <glm/vec3.hpp>

#include "Engine/Input/input.h"

namespace engine::renderer {

class Camera;

class OrbitCameraController {
 public:
  explicit OrbitCameraController(const Camera& camera);

  void Update(Camera& camera, const engine::input::InputState& input,
              float delta_seconds);

 private:
  void ApplyTo(Camera& camera) const;
  void Reset(Camera& camera);

  glm::vec3 target_{0.0F};
  glm::vec3 initial_target_{0.0F};
  float distance_ = 1.0F;
  float yaw_ = 0.0F;
  float pitch_ = 0.0F;
  float initial_distance_ = 1.0F;
  float initial_yaw_ = 0.0F;
  float initial_pitch_ = 0.0F;
};

}  // namespace engine::renderer
