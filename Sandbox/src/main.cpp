#include "Engine/Core/application.h"

int main() {
  Engine::Application app;

  if (!app.Initialize()) {
    return 1;
  }

  app.Run();
  return 0;
}
