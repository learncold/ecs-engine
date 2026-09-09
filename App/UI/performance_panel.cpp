#include "App/UI/performance_panel.h"

#include <imgui.h>

namespace app::ui {

bool PerformancePanel::Draw(std::size_t boid_count,
                            bool& vsync_enabled) const {
  const ImGuiIO& io = ImGui::GetIO();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 panel_position{
      viewport->WorkPos.x + viewport->WorkSize.x - 10.0F,
      viewport->WorkPos.y + 10.0F};

  ImGui::SetNextWindowPos(panel_position, ImGuiCond_Always, ImVec2{1.0F, 0.0F});
  ImGui::SetNextWindowBgAlpha(0.85F);

  constexpr ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

  bool vsync_changed = false;
  if (ImGui::Begin("Performance", nullptr, flags)) {
    const float frame_time_ms =
        io.Framerate > 0.0F ? 1000.0F / io.Framerate : 0.0F;
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame: %.2f ms", frame_time_ms);
    ImGui::Text("Boids: %zu", boid_count);
    ImGui::Separator();
    vsync_changed = ImGui::Checkbox("VSync", &vsync_enabled);
  }
  ImGui::End();
  return vsync_changed;
}

}  // namespace app::ui
