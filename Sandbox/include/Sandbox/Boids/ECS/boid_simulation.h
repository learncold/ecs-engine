#pragma once

#include <cstddef>
#include <cstdint>

#include "Engine/ECS/registry.h"
#include "Engine/ECS/system.h"
#include "Sandbox/Boids/ECS/Components/boid.h"

struct BoidSimulationConfig {
  std::size_t boid_count{100U};
  std::uint32_t random_seed{42U};
  float spawn_half_extent{10.0F};
  float boundary_half_extent{25.0F};
  float initial_speed{2.0F};
  Boid boid{.neighbor_radius = 5.0F,
            .separation_radius = 1.0F,
            .separation_weight = 1.5F,
            .alignment_weight = 1.0F,
            .cohesion_weight = 1.0F,
            .max_speed = 5.0F};
};

class BoidSimulation {
 public:
  explicit BoidSimulation(BoidSimulationConfig config = {});

  void Initialize();
  void Update(float delta_seconds);

  [[nodiscard]] engine::ecs::Registry& GetRegistry() { return registry_; }
  [[nodiscard]] const engine::ecs::Registry& GetRegistry() const {
    return registry_;
  }

 private:
  BoidSimulationConfig config_;
  engine::ecs::Registry registry_;
  engine::ecs::SystemManager system_manager_;
};
