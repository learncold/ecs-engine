#pragma once

struct GLFWwindow;

namespace engine::debug_ui {

class DebugUi {
 public:
  DebugUi() = default;
  ~DebugUi();

  DebugUi(const DebugUi&) = delete;
  DebugUi& operator=(const DebugUi&) = delete;

  bool Initialize(GLFWwindow* window);
  void BeginFrame();
  void Render();
  void Shutdown();

  [[nodiscard]] bool WantsCaptureKeyboard() const;
  [[nodiscard]] bool WantsCaptureMouse() const;

 private:
  bool initialized_{false};
};

}  // namespace engine::debug_ui
