#include "Boids/OOP/boid_simulation.h"

#include <cassert>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "Boids/OOP/boid_object.h"
#include "Boids/OOP/dummy_object.h"

namespace boids::oop {
namespace {

constexpr auto kMissingIndex = std::numeric_limits<std::size_t>::max();

bool IsFinite(const glm::vec3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

std::size_t DummyChangeCount(const boids::BoidSimulationConfig& config) {
  // Avoid overflowing dummy_count * percent.
  return config.dummy_count / 100U * config.dummy_change_percent +
         config.dummy_count % 100U * config.dummy_change_percent / 100U;
}

}  // namespace

BoidSimulation::BoidSimulation(boids::BoidSimulationConfig config)
    : config_(std::move(config)) {
  if (!std::isfinite(config_.boundary_half_extent) ||
      config_.boundary_half_extent <= 0.0F) {
    throw std::invalid_argument(
        "Boundary half extent must be finite and positive");
  }
  if (!std::isfinite(config_.spawn_half_extent) ||
      config_.spawn_half_extent < 0.0F ||
      config_.spawn_half_extent > config_.boundary_half_extent ||
      config_.spawn_half_extent > std::numeric_limits<float>::max() / 2.0F) {
    throw std::invalid_argument(
        "Spawn range must be finite, representable and inside the boundary");
  }
  const auto& parameters = config_.parameters;
  for (const float value :
       {config_.initial_speed, parameters.neighbor_radius,
        parameters.separation_radius, parameters.separation_weight,
        parameters.alignment_weight, parameters.cohesion_weight,
        parameters.preferred_speed, parameters.max_speed,
        parameters.max_alignment_force}) {
    if (!std::isfinite(value) || value < 0.0F) {
      throw std::invalid_argument(
          "Initial speed and Boid parameters must be finite and nonnegative");
    }
  }
  if (config_.dummy_change_percent > 100U) {
    throw std::invalid_argument("Dummy change percent must be in [0, 100]");
  }
  if (config_.boid_count > kMissingIndex - config_.dummy_count) {
    throw std::overflow_error("Object count overflow");
  }
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
    if (!IsFinite(initial.position) || !IsFinite(initial.velocity)) {
      throw std::invalid_argument("Input position and velocity must be finite");
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

void BoidSimulation::Initialize() {
  auto input = std::make_unique<boids::SimulationInput>(
      boids::PrepareSimulationInput(config_, 0U));
  Initialize(*input);
  owned_input_ = std::move(input);
}

void BoidSimulation::Initialize(const boids::SimulationInput& input) {
  ValidateInput(input);
  initialized_ = false;
  input_ = nullptr;
  objects_.clear();
  object_indices_.clear();
  if (owned_input_.get() != &input) owned_input_.reset();

  objects_.reserve(input.initial_objects.size());
  object_indices_.resize(input.initial_objects.size(), kMissingIndex);
  for (const auto& initial : input.initial_objects) {
    if (initial.kind == boids::ObjectKind::Boid) {
      objects_.push_back(
          std::make_unique<BoidObject>(initial, config_.parameters));
    } else {
      objects_.push_back(std::make_unique<DummyObject>(initial));
    }
    object_indices_[initial.logical_id] = objects_.size() - 1U;
  }
  input_ = &input;
  initialized_ = true;
}

void BoidSimulation::ChangeDummies(std::span<const std::size_t> dummy_ids) {
  // A failed allocation during recreation requires reinitialization before the
  // next update. Never continue with an incomplete logical-ID mapping.
  initialized_ = false;
  for (const auto id : dummy_ids) {
    assert(id < object_indices_.size());
    const auto index = object_indices_[id];
    assert(index < objects_.size());
    assert(objects_[index]->Kind() == boids::ObjectKind::Dummy);
    if (index != objects_.size() - 1U) {
      objects_[index] = std::move(objects_.back());
      object_indices_[objects_[index]->LogicalId()] = index;
    }
    objects_.pop_back();
    object_indices_[id] = kMissingIndex;
  }
  for (const auto id : dummy_ids) {
    objects_.push_back(
        std::make_unique<DummyObject>(input_->initial_objects[id]));
    object_indices_[id] = objects_.size() - 1U;
  }
  initialized_ = true;
}

void BoidSimulation::Update(float delta_seconds) {
  engine::measurement::TimingRecorder disabled;
  disabled.set_enabled(false);
  Update(delta_seconds, disabled);
}

void BoidSimulation::Update(float delta_seconds,
                            engine::measurement::TimingRecorder& timing) {
  if (DummyChangeCount(config_) != 0U) {
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
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F) {
    throw std::invalid_argument("Delta time must be finite and nonnegative");
  }

  if (dummy_ids.empty()) {
    timing.record_zero("entity_changes");
  } else {
    timing.start("entity_changes");
    ChangeDummies(dummy_ids);
    timing.stop("entity_changes");
  }
  timing.record_zero("query_preparation");
  timing.start("neighbor_calculation");
  for (auto& object : objects_) {
    if (object->Kind() == boids::ObjectKind::Boid) {
      static_cast<BoidObject&>(*object).CalculateAcceleration(objects_);
    }
  }
  timing.stop("neighbor_calculation");

  timing.start("state_update");
  for (auto& object : objects_) {
    if (object->Kind() == boids::ObjectKind::Boid) {
      static_cast<BoidObject&>(*object).Move(delta_seconds);
    }
  }
  for (auto& object : objects_) {
    if (object->Kind() == boids::ObjectKind::Boid) {
      static_cast<BoidObject&>(*object).ApplyBoundary(
          config_.boundary_half_extent);
    }
  }
  for (auto& object : objects_) {
    if (object->Kind() == boids::ObjectKind::Boid) {
      static_cast<BoidObject&>(*object).UpdateOrientation();
    }
  }
  timing.stop("state_update");
}

engine::measurement::StorageReport BoidSimulation::GetStorageReport() const {
  engine::measurement::StorageReport report;
  report.add_objects<BoidSimulation>("simulation.object", 1U);
  report.add_vector("objects.pointers", objects_);
  report.add_vector("objects.logical_indices", object_indices_);
  std::size_t boid_count = 0U;
  for (const auto& object : objects_) {
    if (object->Kind() == boids::ObjectKind::Boid) ++boid_count;
  }
  report.add_objects<BoidObject>("objects.boids", boid_count);
  report.add_objects<DummyObject>("objects.dummies",
                                  objects_.size() - boid_count);
  // Borrowed input belongs to the caller. The convenience Initialize() owns a
  // separate heap object and its element storage, all counted exactly once.
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

}  // namespace boids::oop
