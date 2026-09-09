#include "Engine/DebugUI/debug_ui.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace engine::debug_ui {

DebugUi::~DebugUi() { Shutdown(); }

bool DebugUi::Initialize(GLFWwindow* window) {
  Shutdown();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::StyleColorsDark();

  if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
    ImGui::DestroyContext();
    return false;
  }

  if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    return false;
  }

  initialized_ = true;
  return true;
}

void DebugUi::BeginFrame() {
  if (!initialized_) {
    return;
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void DebugUi::Render() {
  if (!initialized_) {
    return;
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DebugUi::Shutdown() {
  if (!initialized_) {
    return;
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  initialized_ = false;
}

bool DebugUi::WantsCaptureKeyboard() const {
  return initialized_ && ImGui::GetIO().WantCaptureKeyboard;
}

bool DebugUi::WantsCaptureMouse() const {
  return initialized_ && ImGui::GetIO().WantCaptureMouse;
}

}  // namespace engine::debug_ui
