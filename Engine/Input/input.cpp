#include "Engine/Input/input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace engine::input {

Input::~Input() { Shutdown(); }

void Input::Initialize(GLFWwindow* window) {
  Shutdown();
  window_ = window;
  state_ = {};
  pending_scroll_delta_ = 0.0F;

  glfwSetWindowUserPointer(window_, this);
  glfwSetScrollCallback(window_, ScrollCallback);
  glfwGetCursorPos(window_, &previous_cursor_x_, &previous_cursor_y_);
}

void Input::Update(bool keyboard_enabled, bool mouse_enabled) {
  state_.move_forward =
      keyboard_enabled && glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS;
  state_.move_backward =
      keyboard_enabled && glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS;
  state_.move_left =
      keyboard_enabled && glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS;
  state_.move_right =
      keyboard_enabled && glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS;
  state_.move_up =
      keyboard_enabled && glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS;
  state_.move_down =
      keyboard_enabled && glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS;
  state_.reset_camera =
      keyboard_enabled && glfwGetKey(window_, GLFW_KEY_R) == GLFW_PRESS;

  const bool orbit_button_down =
      mouse_enabled &&
      glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  double cursor_x = 0.0;
  double cursor_y = 0.0;
  glfwGetCursorPos(window_, &cursor_x, &cursor_y);

  if (orbit_button_down != state_.orbit_button_down) {
    glfwSetInputMode(window_, GLFW_CURSOR,
                     orbit_button_down ? GLFW_CURSOR_DISABLED
                                       : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    state_.mouse_delta_x = 0.0F;
    state_.mouse_delta_y = 0.0F;
  } else if (orbit_button_down) {
    state_.mouse_delta_x =
        static_cast<float>(cursor_x - previous_cursor_x_);
    state_.mouse_delta_y =
        static_cast<float>(cursor_y - previous_cursor_y_);
  } else {
    state_.mouse_delta_x = 0.0F;
    state_.mouse_delta_y = 0.0F;
  }

  state_.orbit_button_down = orbit_button_down;
  state_.scroll_delta = mouse_enabled ? pending_scroll_delta_ : 0.0F;
  pending_scroll_delta_ = 0.0F;
  previous_cursor_x_ = cursor_x;
  previous_cursor_y_ = cursor_y;
}

void Input::Shutdown() {
  if (window_ == nullptr) {
    return;
  }

  if (glfwGetWindowUserPointer(window_) == this) {
    glfwSetScrollCallback(window_, nullptr);
    glfwSetWindowUserPointer(window_, nullptr);
  }
  glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  window_ = nullptr;
  state_ = {};
  pending_scroll_delta_ = 0.0F;
}

void Input::ScrollCallback(GLFWwindow* window, double /* x_offset */,
                           double y_offset) {
  auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
  if (input != nullptr) {
    input->pending_scroll_delta_ += static_cast<float>(y_offset);
  }
}

}  // namespace engine::input
