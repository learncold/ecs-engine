#include "Engine/ECSRenderer/ecs_render_system.h"

#include "Engine/ECS/registry.h"
#include "Engine/Renderer/agent_render_manager.h"
#include "Engine/Renderer/mesh_renderer.h"
#include "Engine/Renderer/transform.h"

namespace engine::ecs_renderer {

void EcsRenderSystem::Extract(engine::ecs::Registry& registry) {
  instances_.clear();
  instances_.reserve(registry.EntityCount());

  registry
      .CreateView<engine::renderer::Transform, engine::renderer::MeshRenderer>()
      .Each([this](const engine::renderer::Transform& transform,
                   const engine::renderer::MeshRenderer& mesh_renderer) {
        instances_.push_back(
            {.transform = transform, .mesh = mesh_renderer.mesh});
      });
}

void EcsRenderSystem::Render(engine::ecs::Registry& registry,
                             engine::renderer::AgentRenderManager& render_manager,
                             int framebuffer_width,
                             int framebuffer_height) {
  Extract(registry);
  render_manager.Render(instances_, framebuffer_width, framebuffer_height);
}

}  // namespace engine::ecs_renderer
