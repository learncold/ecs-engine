#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "Boids/Common/boid_simulation_config.h"
#include "Boids/Common/simulation_input.h"
#include "Engine/ECS/registry.h"
#include "Engine/ECS/system.h"

namespace boids::ecs {

class BoidSystem;

class BoidSimulation {
 public:
  explicit BoidSimulation(boids::BoidSimulationConfig config = {});

  void Initialize();
  // Borrow unchanged input until the last Update and storage report.
  void Initialize(const boids::SimulationInput& input);
  void Update(float delta_seconds);
  void Update(float delta_seconds,
              engine::measurement::TimingRecorder& timing_recorder);
  void Update(float delta_seconds, std::span<const std::size_t> dummy_ids,
              engine::measurement::TimingRecorder& timing_recorder);
  [[nodiscard]] engine::ecs::Entity EntityFor(std::size_t logical_id) const;
  [[nodiscard]] engine::measurement::StorageReport GetStorageReport() const;

  [[nodiscard]] engine::ecs::Registry& GetRegistry() { return registry_; }
  [[nodiscard]] const engine::ecs::Registry& GetRegistry() const {
    return registry_;
  }

 private:
  void ValidateInput(const boids::SimulationInput& input) const;
  [[nodiscard]] engine::ecs::Entity CreateObject(
      const boids::InitialObjectState& initial);
  void ChangeDummies(std::span<const std::size_t> dummy_ids);

  boids::BoidSimulationConfig config_;
  engine::ecs::Registry registry_;
  engine::ecs::SystemManager system_manager_;
  engine::ecs::SystemManager state_system_manager_;
  // Owned by system_manager_; valid for the simulation's lifetime.
  BoidSystem* boid_system_ = nullptr;
  std::unique_ptr<boids::SimulationInput> owned_input_;
  const boids::SimulationInput* input_ = nullptr;
  std::vector<engine::ecs::Entity> entities_;
  bool initialized_ = false;
};

}  // namespace boids::ecs
