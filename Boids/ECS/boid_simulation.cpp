#include "Boids/ECS/boid_simulation.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "Boids/Common/simulation_validation.h"
#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/Systems/boid_orientation_system.h"
#include "Boids/ECS/Systems/boid_system.h"
#include "Boids/ECS/Systems/boundary_system.h"
#include "Boids/ECS/Systems/movement_system.h"
#include "Engine/Renderer/mesh_renderer.h"
#include "Engine/Renderer/transform.h"

namespace boids::ecs {

BoidSimulation::BoidSimulation(boids::BoidSimulationConfig config)
    : config_(config) {
  boids::ValidateSimulationConfig(config_);
  const std::size_t limit = engine::ecs::Entity::kInvalidId;
  if (config_.dummy_count > limit ||
      config_.boid_count > limit - config_.dummy_count) {
    throw std::overflow_error("Entity count exceeds ID capacity");
  }

  boid_system_ = &system_manager_.Add<BoidSystem>();
  state_system_manager_.AddTimed<MovementSystem>("state_update");
  state_system_manager_.AddTimed<BoundarySystem>("state_update",
                                                 config_.boundary_half_extent);
  state_system_manager_.AddTimed<BoidOrientationSystem>("state_update");
}

void BoidSimulation::ValidateInput(const boids::SimulationInput& input) const {
  if (input.initial_objects.size() !=
      config_.boid_count + config_.dummy_count) {
    throw std::invalid_argument("Input object count does not match config");
  }
  std::size_t boid_count = 0U;
  std::size_t dummy_count = 0U;
  for (std::size_t index = 0; index < input.initial_objects.size(); ++index) {
    const auto& initial = input.initial_objects[index];
    if (initial.logical_id != index) {
      throw std::invalid_argument(
          "Input logical ID must equal its initial index");
    }
    for (int axis = 0; axis < 3; ++axis) {
      if (!std::isfinite(initial.position[axis]) ||
          !std::isfinite(initial.velocity[axis])) {
        throw std::invalid_argument(
            "Input position and velocity must be finite");
      }
    }
    switch (initial.kind) {
      case boids::ObjectKind::Boid:
        ++boid_count;
        break;
      case boids::ObjectKind::Dummy:
        ++dummy_count;
        break;
      default:
        throw std::invalid_argument("Unknown input object kind");
    }
  }
  if (boid_count != config_.boid_count || dummy_count != config_.dummy_count) {
    throw std::invalid_argument("Input object kinds do not match config");
  }
}

engine::ecs::Entity BoidSimulation::CreateObject(
    const boids::InitialObjectState& initial) {
  const auto entity = registry_.Create();
  registry_.Emplace<engine::renderer::Transform>(
      entity,
      engine::renderer::Transform{
          .position = initial.position,
          .rotation = engine::renderer::RotationFromDirection(initial.velocity),
          .scale = 0.7F});
  registry_.Emplace<Velocity>(entity, Velocity{initial.velocity});
  if (initial.kind == boids::ObjectKind::Boid) {
    registry_.Emplace<Acceleration>(entity, Acceleration{});
    registry_.Emplace<BoidParameters>(entity, config_.parameters);
  }
  return entity;
}

void BoidSimulation::Initialize() {
  auto input = std::make_unique<boids::SimulationInput>(
      boids::PrepareSimulationInput(config_, 0U));
  Initialize(*input);
  owned_input_ = std::move(input);
  initialized_ = false;
  // Retain the existing visualization path without adding render components
  // to the borrowed research-input path or to Dummies.
  for (const auto& initial : input_->initial_objects) {
    if (initial.kind == boids::ObjectKind::Boid) {
      registry_.Emplace<engine::renderer::MeshRenderer>(
          entities_[initial.logical_id]);
    }
  }
  initialized_ = true;
}

void BoidSimulation::Initialize(const boids::SimulationInput& input) {
  ValidateInput(input);
  initialized_ = false;
  input_ = nullptr;
  entities_.clear();
  registry_.Clear();
  if (owned_input_.get() != &input) owned_input_.reset();
  entities_.reserve(input.initial_objects.size());
  for (const auto& initial : input.initial_objects) {
    entities_.push_back(CreateObject(initial));
  }
  input_ = &input;
  initialized_ = true;
}

engine::ecs::Entity BoidSimulation::EntityFor(std::size_t logical_id) const {
  if (logical_id >= entities_.size() ||
      !registry_.IsAlive(entities_[logical_id])) {
    throw std::out_of_range("No Entity for logical ID");
  }
  return entities_[logical_id];
}

void BoidSimulation::ChangeDummies(std::span<const std::size_t> dummy_ids) {
  for (const auto id : dummy_ids) {
    const auto entity = EntityFor(id);
    if (input_->initial_objects[id].kind != boids::ObjectKind::Dummy) {
      throw std::invalid_argument("Only Dummy entities may be replaced");
    }
    registry_.Destroy(entity);
    entities_[id] = engine::ecs::Entity{};
  }
  for (const auto id : dummy_ids) {
    entities_[id] = CreateObject(input_->initial_objects[id]);
  }
}

void BoidSimulation::Update(float delta_seconds) {
  engine::measurement::TimingRecorder disabled;
  disabled.set_enabled(false);
  Update(delta_seconds, disabled);
}

void BoidSimulation::Update(float delta_seconds,
                            engine::measurement::TimingRecorder& timing) {
  const auto change_count =
      config_.dummy_count / 100U * config_.dummy_change_percent +
      config_.dummy_count % 100U * config_.dummy_change_percent / 100U;
  if (change_count != 0U) {
    throw std::logic_error("Dummy changes require an explicit step plan");
  }
  Update(delta_seconds, {}, timing);
}

void BoidSimulation::Update(float delta_seconds,
                            std::span<const std::size_t> dummy_ids,
                            engine::measurement::TimingRecorder& timing) {
  if (!initialized_) {
    throw std::logic_error("Simulation must be initialized before Update");
  }
  boids::ValidateDeltaTime(delta_seconds);
  // Failed structural/numeric work may leave partial changes. Reinitialize
  // and discard the failed measurement before continuing.
  initialized_ = false;
  if (dummy_ids.empty()) {
    timing.record_zero("entity_changes");
  } else {
    timing.start("entity_changes");
    ChangeDummies(dummy_ids);
    timing.stop("entity_changes");
  }
  system_manager_.Update(registry_, delta_seconds, timing);
  if (boid_system_->HasBoids()) {
    state_system_manager_.Update(registry_, delta_seconds, timing);
  } else {
    timing.record_zero("state_update");
  }
  initialized_ = true;
}

engine::measurement::StorageReport BoidSimulation::GetStorageReport() const {
  engine::measurement::StorageReport report;
  report.add_objects<BoidSimulation>("simulation.object", 1U);
  report.add_vector("entities.logical_ids", entities_);
  registry_.AppendStorageReport(report);
  system_manager_.AppendStorageReport(report);
  state_system_manager_.AppendStorageReport(report, "state_systems");
  if (owned_input_) {
    report.add_objects<boids::SimulationInput>("input.object", 1U);
    report.add_vector("input.initial_objects", owned_input_->initial_objects);
    report.add_vector("input.dummy_steps", owned_input_->dummy_steps);
    for (std::size_t step = 0; step < owned_input_->dummy_steps.size();
         ++step) {
      report.add_vector("input.dummy_steps." + std::to_string(step),
                        owned_input_->dummy_steps[step]);
    }
  }
  return report;
}

}  // namespace boids::ecs
