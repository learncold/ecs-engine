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
  float initial_speed{4.0F};
  Boid boid{.neighbor_radius = 6.0F,
            .separation_radius = 2.5F,
            .separation_weight = 2.5F,
            .alignment_weight = 1.0F,
            .cohesion_weight = 0.2F,
            .preferred_speed = 4.0F,
            .max_speed = 6.0F,
            .max_force = 1.5F};
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
