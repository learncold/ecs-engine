#pragma once

#include "Engine/Renderer/mesh_renderer.h"
#include "Engine/Renderer/transform.h"

namespace engine::renderer {

struct RenderInstance {
  engine::renderer::Transform transform;
  engine::renderer::MeshKind mesh{engine::renderer::MeshKind::Agent};
};

}  // namespace engine::renderer
