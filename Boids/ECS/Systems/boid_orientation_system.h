#pragma once

#include "Engine/ECS/system.h"

namespace boids::ecs {

class BoidOrientationSystem final : public engine::ecs::System {
 public:
  void Update(engine::ecs::Registry& registry, float delta_seconds) override;
};

}  // namespace boids::ecs
