#pragma once

#include <memory>
#include <span>
#include <vector>

#include "Boids/Common/boid_simulation_config.h"
#include "Boids/Common/simulation_input.h"
#include "Boids/OOP/simulation_object.h"
#include "Engine/Measurement/memory_measurement.h"
#include "Engine/Measurement/timing_recorder.h"

namespace boids::oop {

class BoidSimulation {
 public:
  explicit BoidSimulation(boids::BoidSimulationConfig config = {});

  BoidSimulation(const BoidSimulation&) = delete;
  BoidSimulation& operator=(const BoidSimulation&) = delete;

  void Initialize();
  // The caller keeps input alive and unchanged through the last Update/report.
  void Initialize(const boids::SimulationInput& input);
  void Update(float delta_seconds);
  void Update(float delta_seconds, engine::measurement::TimingRecorder& timing);
  // dummy_ids is a valid, unique Dummy list prepared by Common; no plan cursor
  // or random selection is maintained by the simulation.
  void Update(float delta_seconds, std::span<const std::size_t> dummy_ids,
              engine::measurement::TimingRecorder& timing);
  [[nodiscard]] engine::measurement::StorageReport GetStorageReport() const;

  [[nodiscard]] std::span<const std::unique_ptr<SimulationObject>> Objects()
      const {
    return objects_;
  }

 private:
  void ValidateInput(const boids::SimulationInput& input) const;
  void ChangeDummies(std::span<const std::size_t> dummy_ids);

  boids::BoidSimulationConfig config_;
  std::unique_ptr<boids::SimulationInput> owned_input_;
  const boids::SimulationInput* input_{nullptr};
  std::vector<std::unique_ptr<SimulationObject>> objects_;
  std::vector<std::size_t> object_indices_;
  bool initialized_{false};
};

}  // namespace boids::oop
