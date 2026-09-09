#pragma once

#include <cstddef>
#include <cstdint>

#include "Boids/Common/boid_parameters.h"

namespace boids {

struct BoidSimulationConfig {
  std::size_t boid_count{100U};
  std::uint32_t random_seed{42U};
  float spawn_half_extent{10.0F};
  float boundary_half_extent{25.0F};
  float initial_speed{4.0F};
  BoidParameters parameters{.neighbor_radius = 6.0F,
                            .separation_radius = 2.5F,
                            .separation_weight = 2.5F,
                            .alignment_weight = 1.0F,
                            .cohesion_weight = 0.2F,
                            .preferred_speed = 4.0F,
                            .max_speed = 6.0F,
                            .max_alignment_force = 1.5F};
};

}  // namespace boids
