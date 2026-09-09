#pragma once

namespace boids {

// Shared behavior parameters; also stored directly as an ECS component.
struct BoidParameters {
  float neighbor_radius;
  float separation_radius;
  float separation_weight;
  float alignment_weight;
  float cohesion_weight;
  float preferred_speed;
  float max_speed;
  // Alignment steering magnitude limit before applying alignment_weight.
  // Separation, cohesion, and the combined acceleration are not clamped here.
  float max_alignment_force;
};

}  // namespace boids
