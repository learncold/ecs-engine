#pragma once

#include "Engine/ECS/system.h"

class MovementSystem final : public engine::ecs::System {
 public:
  void Update(engine::ecs::Registry& registry, float delta_seconds) override;
};