#pragma once

#include <functional>

struct GLFWwindow;

namespace Engine {

class Application {
 public:
  using UpdateCallback = std::function<void(float)>;

  Application();
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool Initialize();
  void Run(const UpdateCallback& update_callback);
  void Shutdown();

 private:
  GLFWwindow* window_ = nullptr;
};

}  // namespace Engine
