#include <gtest/gtest.h>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "Engine/Input/input.h"
#include "Engine/Renderer/camera.h"
#include "Engine/Renderer/orbit_camera_controller.h"

namespace {

constexpr float kTolerance = 0.00001F;

void ExpectVec3Near(const glm::vec3& actual, const glm::vec3& expected) {
  EXPECT_NEAR(actual.x, expected.x, kTolerance);
  EXPECT_NEAR(actual.y, expected.y, kTolerance);
  EXPECT_NEAR(actual.z, expected.z, kTolerance);
}

engine::renderer::Camera MakeCamera() {
  return engine::renderer::Camera({0.0F, 35.0F, 90.0F},
                                  {0.0F, 0.0F, 0.0F}, 50.0F, 0.1F, 250.0F);
}

TEST(OrbitCameraControllerTest, PreservesStateWithoutInput) {
  engine::renderer::Camera camera = MakeCamera();
  engine::renderer::OrbitCameraController controller(camera);
  const glm::vec3 initial_position = camera.Position();
  const glm::vec3 initial_target = camera.Target();

  controller.Update(camera, engine::input::InputState{}, 1.0F / 60.0F);

  ExpectVec3Near(camera.Position(), initial_position);
  ExpectVec3Near(camera.Target(), initial_target);
}

TEST(OrbitCameraControllerTest, OrbitPreservesDistanceAndTarget) {
  engine::renderer::Camera camera = MakeCamera();
  engine::renderer::OrbitCameraController controller(camera);
  const glm::vec3 initial_position = camera.Position();
  const glm::vec3 initial_target = camera.Target();
  const float initial_distance =
      glm::length(initial_position - initial_target);
  engine::input::InputState input;
  input.orbit_button_down = true;
  input.mouse_delta_x = 100.0F;
  input.mouse_delta_y = -20.0F;

  controller.Update(camera, input, 1.0F / 60.0F);

  ExpectVec3Near(camera.Target(), initial_target);
  EXPECT_NEAR(glm::length(camera.Position() - camera.Target()),
              initial_distance, kTolerance);
  EXPECT_GT(glm::length(camera.Position() - initial_position), 1.0F);
}

TEST(OrbitCameraControllerTest, PositiveScrollZoomsIn) {
  engine::renderer::Camera camera = MakeCamera();
  engine::renderer::OrbitCameraController controller(camera);
  const float initial_distance =
      glm::length(camera.Position() - camera.Target());
  engine::input::InputState input;
  input.scroll_delta = 1.0F;

  controller.Update(camera, input, 1.0F / 60.0F);

  EXPECT_LT(glm::length(camera.Position() - camera.Target()),
            initial_distance);
}

TEST(OrbitCameraControllerTest, MovementTranslatesPositionAndTargetTogether) {
  engine::renderer::Camera camera = MakeCamera();
  engine::renderer::OrbitCameraController controller(camera);
  const glm::vec3 initial_position = camera.Position();
  const glm::vec3 initial_target = camera.Target();
  engine::input::InputState input;
  input.move_forward = true;

  controller.Update(camera, input, 1.0F);

  const glm::vec3 position_offset = camera.Position() - initial_position;
  const glm::vec3 target_offset = camera.Target() - initial_target;
  ExpectVec3Near(position_offset, target_offset);
  EXPECT_NEAR(glm::length(target_offset), 30.0F, kTolerance);
}

TEST(OrbitCameraControllerTest, ResetRestoresInitialCamera) {
  engine::renderer::Camera camera = MakeCamera();
  engine::renderer::OrbitCameraController controller(camera);
  const glm::vec3 initial_position = camera.Position();
  const glm::vec3 initial_target = camera.Target();
  engine::input::InputState changed_input;
  changed_input.orbit_button_down = true;
  changed_input.mouse_delta_x = 80.0F;
  changed_input.scroll_delta = 2.0F;
  changed_input.move_right = true;
  controller.Update(camera, changed_input, 0.5F);

  engine::input::InputState reset_input;
  reset_input.reset_camera = true;
  controller.Update(camera, reset_input, 1.0F / 60.0F);

  ExpectVec3Near(camera.Position(), initial_position);
  ExpectVec3Near(camera.Target(), initial_target);
}

}  // namespace
