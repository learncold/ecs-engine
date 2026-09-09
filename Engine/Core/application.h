#pragma once

#include <functional>

#include "Engine/DebugUI/debug_ui.h"
#include "Engine/Input/input.h"

struct GLFWwindow;

namespace engine::core {

class Application {
 public:
  using UpdateCallback =
      std::function<void(float, const engine::input::InputState&)>;
  using RenderCallback = std::function<void(int, int)>;
  using UiCallback = std::function<void()>;

  Application();
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool Initialize();
  void Run(const UpdateCallback& update_callback,
           const RenderCallback& render_callback,
           const UiCallback& ui_callback = {});
  void SetVSyncEnabled(bool enabled);
  [[nodiscard]] bool IsVSyncEnabled() const { return vsync_enabled_; }
  void Shutdown();

 private:
  GLFWwindow* window_ = nullptr;
  engine::input::Input input_;
  engine::debug_ui::DebugUi debug_ui_;
  bool vsync_enabled_{false};
};

}  // namespace engine::core
