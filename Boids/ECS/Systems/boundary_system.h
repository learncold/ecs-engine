#pragma once

#include "Engine/ECS/system.h"

namespace boids::ecs {

class BoundarySystem final : public engine::ecs::System {
 public:
  explicit BoundarySystem(float half_extent);

  void Update(engine::ecs::Registry& registry, float delta_seconds) override;

 private:
  float half_extent_;
};

}  // namespace boids::ecs
