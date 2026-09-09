#pragma once

namespace engine::renderer {

enum class MeshKind {
  Agent,
};

struct MeshRenderer {
  MeshKind mesh{MeshKind::Agent};
};

}  // namespace engine::renderer
