#pragma once

#include <glm/vec3.hpp>

namespace boids::ecs {

struct Acceleration {
  glm::vec3 value{0.0F};
};

}  // namespace boids::ecs
