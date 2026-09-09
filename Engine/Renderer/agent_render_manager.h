#pragma once

#include <span>
#include <vector>

#include <glm/mat4x4.hpp>

#include "Engine/Input/input.h"
#include "Engine/Renderer/agent_renderer.h"
#include "Engine/Renderer/camera.h"
#include "Engine/Renderer/line_renderer.h"
#include "Engine/Renderer/orbit_camera_controller.h"
#include "Engine/Renderer/render_instance.h"

namespace engine::renderer {

class AgentRenderManager {
 public:
  explicit AgentRenderManager(float boundary_half_extent);

  [[nodiscard]] bool Initialize();
  void UpdateCamera(const engine::input::InputState& input,
                    float delta_seconds);
  void Render(std::span<const RenderInstance> instances,
              int framebuffer_width, int framebuffer_height);

 private:
  void BuildEnvironmentLines(float boundary_half_extent);

  AgentRenderer agent_renderer_;
  LineRenderer line_renderer_;
  Camera camera_;
  OrbitCameraController camera_controller_;
  std::vector<glm::mat4> model_matrices_;
  std::vector<LineVertex> environment_lines_;
};

}  // namespace engine::renderer
