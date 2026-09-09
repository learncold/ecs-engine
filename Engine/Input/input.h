#pragma once

struct GLFWwindow;

namespace engine::input {

struct InputState {
  bool move_forward{false};
  bool move_backward{false};
  bool move_left{false};
  bool move_right{false};
  bool move_up{false};
  bool move_down{false};
  bool reset_camera{false};
  bool orbit_button_down{false};
  float mouse_delta_x{0.0F};
  float mouse_delta_y{0.0F};
  float scroll_delta{0.0F};
};

class Input {
 public:
  Input() = default;
  ~Input();

  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;

  void Initialize(GLFWwindow* window);
  void Update(bool keyboard_enabled = true, bool mouse_enabled = true);
  void Shutdown();

  [[nodiscard]] const InputState& State() const { return state_; }

 private:
  static void ScrollCallback(GLFWwindow* window, double x_offset,
                             double y_offset);

  GLFWwindow* window_ = nullptr;
  InputState state_;
  double previous_cursor_x_ = 0.0;
  double previous_cursor_y_ = 0.0;
  float pending_scroll_delta_ = 0.0F;
};

}  // namespace engine::input
