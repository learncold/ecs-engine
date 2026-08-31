#include "Sandbox/Boids/Rendering/boid_renderer.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cmath>

#include "Engine/ECS/registry.h"
#include "Sandbox/Boids/ECS/Components/boid.h"
#include "Sandbox/Boids/ECS/Components/transform.h"
#include "Sandbox/Boids/ECS/Components/velocity.h"

namespace {

void AddLine(std::vector<engine::renderer::LineVertex>& vertices,
             const glm::vec3& start, const glm::vec3& end,
             const glm::vec3& color) {
  vertices.push_back({.position = start, .color = color});
  vertices.push_back({.position = end, .color = color});
}

glm::mat4 BuildModelMatrix(const glm::vec3& position,
                           const glm::vec3& velocity) {
  constexpr float kMinSpeedSquared = 0.00000001F;
  const glm::vec3 forward =
      glm::dot(velocity, velocity) > kMinSpeedSquared
          ? glm::normalize(velocity)
          : glm::vec3{0.0F, 0.0F, 1.0F};

  const glm::vec3 reference_up =
      std::abs(glm::dot(forward, glm::vec3{0.0F, 1.0F, 0.0F})) > 0.99F
          ? glm::vec3{1.0F, 0.0F, 0.0F}
          : glm::vec3{0.0F, 1.0F, 0.0F};
  const glm::vec3 right = glm::normalize(glm::cross(reference_up, forward));
  const glm::vec3 up = glm::cross(forward, right);

  glm::mat4 orientation{1.0F};
  orientation[0] = glm::vec4{right, 0.0F};
  orientation[1] = glm::vec4{up, 0.0F};
  orientation[2] = glm::vec4{forward, 0.0F};

  glm::mat4 model = glm::translate(glm::mat4{1.0F}, position) * orientation;
  return glm::scale(model, glm::vec3{0.7F});
}

}  // namespace

BoidRenderer::BoidRenderer(float boundary_half_extent)
    : camera_({0.0F, 35.0F, 90.0F}, {0.0F, 0.0F, 0.0F}, 50.0F, 0.1F,
              250.0F) {
  BuildEnvironmentLines(boundary_half_extent);
}

bool BoidRenderer::Initialize() {
  return agent_renderer_.Initialize() &&
         line_renderer_.Initialize(environment_lines_);
}

void BoidRenderer::Render(engine::ecs::Registry& registry,
                          int framebuffer_width, int framebuffer_height) {
  model_matrices_.clear();
  model_matrices_.reserve(registry.EntityCount());

  registry.CreateView<Transform, Velocity, Boid>().Each(
      [this](const Transform& transform, const Velocity& velocity,
             const Boid&) {
        model_matrices_.push_back(
            BuildModelMatrix(transform.position, velocity.value));
      });

  const float aspect_ratio = static_cast<float>(framebuffer_width) /
                             static_cast<float>(framebuffer_height);
  const glm::mat4 view_projection = camera_.ViewProjection(aspect_ratio);
  line_renderer_.Render(view_projection);
  agent_renderer_.Render(model_matrices_, view_projection, camera_.Position());
}

void BoidRenderer::BuildEnvironmentLines(float boundary_half_extent) {
  constexpr int kGridDivisions = 10;
  const glm::vec3 grid_color{0.82F, 0.84F, 0.87F};
  const glm::vec3 center_grid_color{0.58F, 0.62F, 0.68F};
  const glm::vec3 boundary_color{0.42F, 0.50F, 0.60F};
  const float floor_y = -boundary_half_extent;
  const float grid_step = (boundary_half_extent * 2.0F) /
                          static_cast<float>(kGridDivisions);

  environment_lines_.clear();
  environment_lines_.reserve(
      static_cast<std::size_t>((kGridDivisions + 1) * 4 + 24));

  for (int index = 0; index <= kGridDivisions; ++index) {
    const float coordinate =
        -boundary_half_extent + static_cast<float>(index) * grid_step;
    const glm::vec3 color = index == kGridDivisions / 2
                                ? center_grid_color
                                : grid_color;
    AddLine(environment_lines_,
            {-boundary_half_extent, floor_y, coordinate},
            {boundary_half_extent, floor_y, coordinate}, color);
    AddLine(environment_lines_, {coordinate, floor_y, -boundary_half_extent},
            {coordinate, floor_y, boundary_half_extent}, color);
  }

  const float low = -boundary_half_extent;
  const float high = boundary_half_extent;
  const glm::vec3 corners[8]{{low, low, low},   {high, low, low},
                             {high, low, high}, {low, low, high},
                             {low, high, low},  {high, high, low},
                             {high, high, high}, {low, high, high}};
  constexpr int kEdges[12][2]{{0, 1}, {1, 2}, {2, 3}, {3, 0},
                               {4, 5}, {5, 6}, {6, 7}, {7, 4},
                               {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (const auto& edge : kEdges) {
    AddLine(environment_lines_, corners[edge[0]], corners[edge[1]],
            boundary_color);
  }
}
