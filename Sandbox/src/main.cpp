#include "Engine/Core/application.h"
#include "Sandbox/Boids/ECS/boid_simulation.h"

int main() {
  Engine::Application app;
  BoidSimulation simulation;

  if (!app.Initialize()) {
    return 1;
  }

  simulation.Initialize();
  app.Run([&simulation](float delta_seconds) {
    simulation.Update(delta_seconds);
  });
  return 0;
}
