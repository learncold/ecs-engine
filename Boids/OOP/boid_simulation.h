#pragma once

#include <span>
#include <vector>

#include "Boids/Common/boid_simulation_config.h"
#include "Boids/OOP/boid_object.h"

namespace boids::oop {

class BoidSimulation {
 public:
  explicit BoidSimulation(boids::BoidSimulationConfig config = {});

  void Initialize();
  void Update(float delta_seconds);

  [[nodiscard]] std::span<const BoidObject> Boids() const { return boids_; }

 private:
  boids::BoidSimulationConfig config_;
  std::vector<BoidObject> boids_;
};

}  // namespace boids::oop
