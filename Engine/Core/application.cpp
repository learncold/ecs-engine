#include "Engine/Core/application.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <chrono>
#include <iostream>

namespace engine::core {
namespace {

void FramebufferSizeCallback(GLFWwindow*, int width, int height) {
  glViewport(0, 0, width, height);
}

}  // namespace

Application::Application() = default;

Application::~Application() { Shutdown(); }

bool Application::Initialize() {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ = glfwCreateWindow(1280, 720, "ECS Engine", nullptr, nullptr);
  if (window_ == nullptr) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window_);
  glfwSetFramebufferSizeCallback(window_, FramebufferSizeCallback);
  glfwSwapInterval(0);

  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    std::cerr << "Failed to initialize GLAD\n";
    return false;
  }

  glViewport(0, 0, 1280, 720);
  input_.Initialize(window_);

  if (!debug_ui_.Initialize(window_)) {
    std::cerr << "Failed to initialize debug UI\n";
    return false;
  }

  return true;
}

void Application::Run(const UpdateCallback& update_callback,
                      const RenderCallback& render_callback,
                      const UiCallback& ui_callback) {
  auto previous_time = std::chrono::steady_clock::now();

  while (!glfwWindowShouldClose(window_)) {
    const auto current_time = std::chrono::steady_clock::now();
    const float delta_seconds =
        std::chrono::duration<float>(current_time - previous_time).count();
    previous_time = current_time;

    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    debug_ui_.BeginFrame();
    input_.Update(!debug_ui_.WantsCaptureKeyboard(),
                  !debug_ui_.WantsCaptureMouse());
    update_callback(delta_seconds, input_.State());

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window_, &framebuffer_width, &framebuffer_height);

    glClearColor(1.0F, 1.0F, 1.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (framebuffer_width > 0 && framebuffer_height > 0) {
      render_callback(framebuffer_width, framebuffer_height);
    }

    if (ui_callback) {
      ui_callback();
    }
    debug_ui_.Render();

    glfwSwapBuffers(window_);
    glfwPollEvents();
  }
}

void Application::SetVSyncEnabled(bool enabled) {
  if (window_ == nullptr || vsync_enabled_ == enabled) {
    return;
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(enabled ? 1 : 0);
  vsync_enabled_ = enabled;
}

void Application::Shutdown() {
  if (window_ != nullptr) {
    debug_ui_.Shutdown();
    input_.Shutdown();
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }

  glfwTerminate();
}

}  // namespace engine::core
