#pragma once

#include <span>
#include <vector>

#include "Engine/Renderer/render_instance.h"

namespace engine::ecs {
class Registry;
}

namespace engine::renderer {
class AgentRenderManager;
}

namespace engine::ecs_renderer {

class EcsRenderSystem {
 public:
  void Extract(engine::ecs::Registry& registry);
  void Render(engine::ecs::Registry& registry,
              engine::renderer::AgentRenderManager& render_manager,
              int framebuffer_width, int framebuffer_height);

  [[nodiscard]] std::span<const engine::renderer::RenderInstance> Instances()
      const {
    return instances_;
  }

 private:
  std::vector<engine::renderer::RenderInstance> instances_;
};

}  // namespace engine::ecs_renderer
