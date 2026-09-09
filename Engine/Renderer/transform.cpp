#include "Engine/Renderer/transform.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace engine::renderer {

namespace {

constexpr float kMinDirectionLengthSquared = 0.00000001F;
constexpr glm::vec3 kWorldUp{0.0F, 1.0F, 0.0F};

}  // namespace

glm::mat4 ToModelMatrix(const Transform& transform) {
  const glm::mat4 translation =
      glm::translate(glm::mat4{1.0F}, transform.position);
  const glm::mat4 rotation = glm::mat4_cast(glm::normalize(transform.rotation));
  const glm::mat4 scale =
      glm::scale(glm::mat4{1.0F}, glm::vec3{transform.scale});
  return translation * rotation * scale;
}

glm::quat RotationFromDirection(const glm::vec3& direction) {
  if (glm::dot(direction, direction) <= kMinDirectionLengthSquared) {
    return glm::quat{1.0F, 0.0F, 0.0F, 0.0F};
  }

  const glm::vec3 forward = glm::normalize(direction);
  const glm::vec3 reference_up =
      std::abs(glm::dot(forward, kWorldUp)) > 0.99F
          ? glm::vec3{1.0F, 0.0F, 0.0F}
          : kWorldUp;
  const glm::vec3 right = glm::normalize(glm::cross(reference_up, forward));
  const glm::vec3 up = glm::cross(forward, right);

  glm::mat3 orientation{1.0F};
  orientation[0] = right;
  orientation[1] = up;
  orientation[2] = forward;
  return glm::normalize(glm::quat_cast(orientation));
}

}  // namespace engine::renderer
