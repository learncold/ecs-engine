#pragma once

#include "Boids/OOP/simulation_object.h"

namespace boids::oop {

class DummyObject final : public SimulationObject {
 public:
  explicit DummyObject(const boids::InitialObjectState& initial)
      : SimulationObject(boids::ObjectKind::Dummy, initial.logical_id,
                         initial.position, initial.velocity) {}
};

}  // namespace boids::oop
