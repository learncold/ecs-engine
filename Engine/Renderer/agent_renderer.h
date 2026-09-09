#pragma once

#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine::renderer {

class AgentRenderer {
 public:
  AgentRenderer() = default;
  ~AgentRenderer();

  AgentRenderer(const AgentRenderer&) = delete;
  AgentRenderer& operator=(const AgentRenderer&) = delete;

  [[nodiscard]] bool Initialize();
  void Render(std::span<const glm::mat4> model_matrices,
              const glm::mat4& view_projection,
              const glm::vec3& camera_position);
  void Shutdown();

 private:
  unsigned int shader_program_ = 0U;
  unsigned int vertex_array_ = 0U;
  unsigned int mesh_buffer_ = 0U;
  unsigned int instance_buffer_ = 0U;
  int view_projection_location_ = -1;
  int camera_position_location_ = -1;
};

}  // namespace engine::renderer
