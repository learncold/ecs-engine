#pragma once

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace engine::renderer {

struct Transform {
  glm::vec3 position{0.0F};
  glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
  float scale{1.0F};
};

[[nodiscard]] glm::mat4 ToModelMatrix(const Transform& transform);

[[nodiscard]] glm::quat RotationFromDirection(const glm::vec3& direction);

}  // namespace engine::renderer
