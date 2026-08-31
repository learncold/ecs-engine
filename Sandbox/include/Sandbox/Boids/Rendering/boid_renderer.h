#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

#include "Engine/Renderer/agent_renderer.h"
#include "Engine/Renderer/camera.h"
#include "Engine/Renderer/line_renderer.h"

namespace engine::ecs {
class Registry;
}

class BoidRenderer {
 public:
  explicit BoidRenderer(float boundary_half_extent);

  [[nodiscard]] bool Initialize();
  void Render(engine::ecs::Registry& registry, int framebuffer_width,
              int framebuffer_height);

 private:
  void BuildEnvironmentLines(float boundary_half_extent);

  engine::renderer::AgentRenderer agent_renderer_;
  engine::renderer::LineRenderer line_renderer_;
  engine::renderer::Camera camera_;
  std::vector<glm::mat4> model_matrices_;
  std::vector<engine::renderer::LineVertex> environment_lines_;
};
