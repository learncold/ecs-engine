#include "Engine/Renderer/camera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <stdexcept>

namespace engine::renderer {

Camera::Camera(glm::vec3 position, glm::vec3 target,
               float vertical_fov_degrees, float near_plane, float far_plane)
    : position_(position),
      target_(target),
      vertical_fov_degrees_(vertical_fov_degrees),
      near_plane_(near_plane),
      far_plane_(far_plane) {
  if (vertical_fov_degrees_ <= 0.0F || vertical_fov_degrees_ >= 180.0F) {
    throw std::invalid_argument("Camera field of view must be between 0 and 180");
  }

  if (near_plane_ <= 0.0F || far_plane_ <= near_plane_) {
    throw std::invalid_argument("Camera clip planes are invalid");
  }
}

glm::mat4 Camera::ViewProjection(float aspect_ratio) const {
  if (aspect_ratio <= 0.0F) {
    throw std::invalid_argument("Camera aspect ratio must be positive");
  }

  const glm::mat4 view = glm::lookAt(position_, target_, {0.0F, 1.0F, 0.0F});
  const glm::mat4 projection =
      glm::perspective(glm::radians(vertical_fov_degrees_), aspect_ratio,
                       near_plane_, far_plane_);
  return projection * view;
}

}  // namespace engine::renderer
