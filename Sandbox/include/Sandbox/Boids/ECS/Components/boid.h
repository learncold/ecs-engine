#pragma once

struct Boid {
  float neighbor_radius;
  float separation_radius;
  float separation_weight;
  float alignment_weight;
  float cohesion_weight;
  float max_speed;
};