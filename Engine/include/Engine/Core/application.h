#pragma once

struct GLFWwindow;

namespace Engine {

class Application {
 public:
  Application();
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool Initialize();
  void Run();
  void Shutdown();

 private:
  GLFWwindow* window_ = nullptr;
};

}  // namespace Engine
