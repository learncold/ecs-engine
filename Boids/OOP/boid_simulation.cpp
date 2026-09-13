#include "Boids/OOP/boid_simulation.h"

#include <random>
#include <stdexcept>
#include <utility>

#include <glm/geometric.hpp>

#include "Boids/OOP/boid_object.h"

namespace boids::oop {

BoidSimulation::BoidSimulation(boids::BoidSimulationConfig config)
    : config_(std::move(config)) {
  if (config_.boundary_half_extent <= 0.0F) {
    throw std::invalid_argument("Boundary half extent must be positive");
  }
  if (config_.spawn_half_extent < 0.0F ||
      config_.spawn_half_extent > config_.boundary_half_extent) {
    throw std::invalid_argument(
        "Spawn half extent must be inside the simulation boundary");
  }
  if (config_.initial_speed < 0.0F) {
    throw std::invalid_argument("Initial speed cannot be negative");
  }
}

void BoidSimulation::Initialize() {
  boids_.clear();
  boids_.reserve(config_.boid_count);

  std::mt19937 random_engine{config_.random_seed};
  std::uniform_real_distribution<float> position_distribution(
      -config_.spawn_half_extent, config_.spawn_half_extent);
  std::uniform_real_distribution<float> direction_distribution(-1.0F, 1.0F);

  for (std::size_t index = 0; index < config_.boid_count; ++index) {
    const glm::vec3 position{position_distribution(random_engine),
                             position_distribution(random_engine),
                             position_distribution(random_engine)};

    glm::vec3 direction{direction_distribution(random_engine),
                        direction_distribution(random_engine),
                        direction_distribution(random_engine)};
    if (glm::dot(direction, direction) <= 0.00000001F) {
      direction = glm::vec3{1.0F, 0.0F, 0.0F};
    } else {
      direction = glm::normalize(direction);
    }
    const glm::vec3 velocity = direction * config_.initial_speed;

    boids_.emplace_back(position, velocity, config_.parameters);
  }
}

void BoidSimulation::Update(float delta_seconds) {
  if (delta_seconds < 0.0F) {
    throw std::invalid_argument("Delta time cannot be negative");
  }

  for (auto& boid : boids_) {
    boid.CalculateAcceleration(boids_);
  }

  for (auto& boid : boids_) {
    boid.Move(delta_seconds);
  }

  for (auto& boid : boids_) {
    boid.ApplyBoundary(config_.boundary_half_extent);
  }

  for (auto& boid : boids_) {
    boid.UpdateOrientation();
  }
}

}  // namespace boids::oop
