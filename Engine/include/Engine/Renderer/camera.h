#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine::renderer {

class Camera {
 public:
  Camera(glm::vec3 position, glm::vec3 target, float vertical_fov_degrees,
         float near_plane, float far_plane);

  [[nodiscard]] glm::mat4 ViewProjection(float aspect_ratio) const;
  [[nodiscard]] const glm::vec3& Position() const { return position_; }

 private:
  glm::vec3 position_;
  glm::vec3 target_;
  float vertical_fov_degrees_;
  float near_plane_;
  float far_plane_;
};

}  // namespace engine::renderer
