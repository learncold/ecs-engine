#include "Boids/ECS/Systems/boid_system.h"

#include <cstddef>
#include <glm/glm.hpp>

#include "Boids/Common/alignment.h"
#include "Boids/Common/vector_math.h"
#include "Boids/ECS/Components/acceleration.h"

namespace boids::ecs {

void BoidSystem::Update(engine::ecs::Registry& registry,
                        float /* delta_seconds */,
                        engine::measurement::TimingRecorder& timing) {
  timing.start("query_preparation");
  if (!neighbor_view_.has_value()) {
    neighbor_view_.emplace(registry);
  } else if (!neighbor_view_->IsCurrent(registry)) {
    neighbor_view_->Refresh(registry);
  }
  timing.stop("query_preparation");

  if (!HasBoids()) {
    timing.record_zero("neighbor_calculation");
    return;
  }
  timing.start("neighbor_calculation");
  registry
      .CreateView<BoidParameters, engine::renderer::Transform, Velocity,
                  Acceleration>()
      .Each([&](engine::ecs::Entity entity, const BoidParameters& parameters,
                const engine::renderer::Transform& transform,
                const Velocity& velocity, Acceleration& acceleration) {
        acceleration.value = CalculateAcceleration(
            *neighbor_view_, entity, transform, velocity, parameters);
      });
  timing.stop("neighbor_calculation");
}

void BoidSystem::AppendStorageReport(
    engine::measurement::StorageReport& report) const {
  if (neighbor_view_) {
    neighbor_view_->AppendStorageReport(report, "neighbors.cached_entries");
  } else {
    report.add_bytes("neighbors.cached_entries", 0U, 0U);
  }
}

glm::vec3 BoidSystem::CalculateAcceleration(
    NeighborView& neighbor_view, engine::ecs::Entity self,
    const engine::renderer::Transform& self_transform,
    const Velocity& self_velocity,
    const BoidParameters& self_parameters) const {
  glm::vec3 separation{0.0F};
  glm::vec3 velocity_sum{0.0F};
  glm::vec3 position_sum{0.0F};
  std::size_t neighbor_count = 0U;

  constexpr float kMinDistanceSquared = 0.00000001F;
  neighbor_view.Each([&](engine::ecs::Entity other, const BoidParameters&,
                         const engine::renderer::Transform& other_transform,
                         const Velocity& other_velocity) {
    if (other == self) {
      return;
    }

    const glm::vec3 offset = other_transform.position - self_transform.position;
    const float distance_squared = glm::dot(offset, offset);
    if (!boids::WithinRadius(distance_squared, self_transform.position,
                             other_transform.position,
                             self_parameters.neighbor_radius)) {
      return;
    }

    ++neighbor_count;
    velocity_sum += other_velocity.value;
    position_sum += other_transform.position;

    if (distance_squared > kMinDistanceSquared &&
        boids::WithinRadius(distance_squared, self_transform.position,
                            other_transform.position,
                            self_parameters.separation_radius)) {
      separation -= boids::InverseSquareOffset(offset, distance_squared);
    }
  });

  if (neighbor_count == 0U) {
    return glm::vec3{0.0F};
  }

  const float count = static_cast<float>(neighbor_count);
  const glm::vec3 alignment = boids::CalculateAlignment(
      velocity_sum, neighbor_count, self_velocity.value, self_parameters);
  const glm::vec3 cohesion = position_sum / count - self_transform.position;
  return boids::RequireFinite(separation * self_parameters.separation_weight +
                              alignment * self_parameters.alignment_weight +
                              cohesion * self_parameters.cohesion_weight);
}

}  // namespace boids::ecs
