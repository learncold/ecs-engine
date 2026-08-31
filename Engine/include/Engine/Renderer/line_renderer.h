#pragma once

#include <cstddef>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine::renderer {

struct LineVertex {
  glm::vec3 position{0.0F};
  glm::vec3 color{0.0F};
};

class LineRenderer {
 public:
  LineRenderer() = default;
  ~LineRenderer();

  LineRenderer(const LineRenderer&) = delete;
  LineRenderer& operator=(const LineRenderer&) = delete;

  [[nodiscard]] bool Initialize(std::span<const LineVertex> vertices);
  void Render(const glm::mat4& view_projection);
  void Shutdown();

 private:
  unsigned int shader_program_ = 0U;
  unsigned int vertex_array_ = 0U;
  unsigned int vertex_buffer_ = 0U;
  int view_projection_location_ = -1;
  std::size_t vertex_count_ = 0U;
};

}  // namespace engine::renderer
