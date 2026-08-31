#include "Engine/Core/application.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <chrono>
#include <iostream>

namespace Engine {
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
  glfwSwapInterval(1);

  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    std::cerr << "Failed to initialize GLAD\n";
    return false;
  }

  glViewport(0, 0, 1280, 720);
  return true;
}

void Application::Run(const UpdateCallback& update_callback) {
  auto previous_time = std::chrono::steady_clock::now();

  while (!glfwWindowShouldClose(window_)) {
    const auto current_time = std::chrono::steady_clock::now();
    const float delta_seconds =
        std::chrono::duration<float>(current_time - previous_time).count();
    previous_time = current_time;

    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    update_callback(delta_seconds);

    glClearColor(1.0F, 1.0F, 1.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    glfwSwapBuffers(window_);
    glfwPollEvents();
  }
}

void Application::Shutdown() {
  if (window_ != nullptr) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }

  glfwTerminate();
}

}  // namespace Engine
