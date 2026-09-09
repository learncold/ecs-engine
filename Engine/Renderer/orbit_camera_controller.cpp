#include "Engine/Renderer/orbit_camera_controller.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>

#include "Engine/Renderer/camera.h"

namespace engine::renderer {

namespace {

constexpr float kRotationRadiansPerPixel = 0.005F;
constexpr float kPanUnitsPerSecond = 30.0F;
constexpr float kZoomFactorPerStep = 0.12F;
constexpr float kMinDistance = 10.0F;
constexpr float kMaxDistance = 180.0F;
constexpr float kMaxPitch = 1.45F;
constexpr float kMinDirectionLengthSquared = 0.00000001F;

}  // namespace

OrbitCameraController::OrbitCameraController(const Camera& camera) {
  target_ = camera.Target();
  const glm::vec3 offset = camera.Position() - target_;
  distance_ = glm::length(offset);
  yaw_ = std::atan2(offset.x, offset.z);
  pitch_ = std::asin(glm::clamp(offset.y / distance_, -1.0F, 1.0F));

  initial_target_ = target_;
  initial_distance_ = distance_;
  initial_yaw_ = yaw_;
  initial_pitch_ = pitch_;
}

void OrbitCameraController::Update(Camera& camera,
                                   const engine::input::InputState& input,
                                   float delta_seconds) {
  if (input.reset_camera) {
    Reset(camera);
    return;
  }

  if (input.orbit_button_down) {
    yaw_ -= input.mouse_delta_x * kRotationRadiansPerPixel;
    pitch_ -= input.mouse_delta_y * kRotationRadiansPerPixel;
    pitch_ = glm::clamp(pitch_, -kMaxPitch, kMaxPitch);
  }

  distance_ *= std::exp(-input.scroll_delta * kZoomFactorPerStep);
  distance_ = glm::clamp(distance_, kMinDistance, kMaxDistance);

  glm::vec3 forward = target_ - camera.Position();
  forward.y = 0.0F;
  if (glm::dot(forward, forward) <= kMinDirectionLengthSquared) {
    forward = glm::vec3{0.0F, 0.0F, -1.0F};
  } else {
    forward = glm::normalize(forward);
  }
  const glm::vec3 right =
      glm::normalize(glm::cross(forward, glm::vec3{0.0F, 1.0F, 0.0F}));

  glm::vec3 movement{0.0F};
  if (input.move_forward) movement += forward;
  if (input.move_backward) movement -= forward;
  if (input.move_right) movement += right;
  if (input.move_left) movement -= right;
  if (input.move_up) movement.y += 1.0F;
  if (input.move_down) movement.y -= 1.0F;

  if (glm::dot(movement, movement) > kMinDirectionLengthSquared) {
    target_ += glm::normalize(movement) * kPanUnitsPerSecond * delta_seconds;
  }

  ApplyTo(camera);
}

void OrbitCameraController::ApplyTo(Camera& camera) const {
  const float horizontal_distance = distance_ * std::cos(pitch_);
  const glm::vec3 offset{horizontal_distance * std::sin(yaw_),
                         distance_ * std::sin(pitch_),
                         horizontal_distance * std::cos(yaw_)};
  camera.SetTarget(target_);
  camera.SetPosition(target_ + offset);
}

void OrbitCameraController::Reset(Camera& camera) {
  target_ = initial_target_;
  distance_ = initial_distance_;
  yaw_ = initial_yaw_;
  pitch_ = initial_pitch_;
  ApplyTo(camera);
}

}  // namespace engine::renderer
