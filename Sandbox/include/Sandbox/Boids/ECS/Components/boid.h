#pragma once

struct Boid {
  float neighbor_radius;
  float separation_radius;
  float separation_weight;
  float alignment_weight;
  float cohesion_weight;
  float preferred_speed;
  float max_speed;
  float max_force;
};
