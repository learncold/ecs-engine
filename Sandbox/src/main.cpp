#include "Engine/Core/application.h"
#include "Sandbox/Boids/ECS/boid_simulation.h"
#include "Sandbox/Boids/Rendering/boid_renderer.h"

int main() {
  Engine::Application app;
  BoidSimulationConfig simulation_config;
  BoidSimulation simulation(simulation_config);
  BoidRenderer renderer(simulation_config.boundary_half_extent);

  if (!app.Initialize()) {
    return 1;
  }

  simulation.Initialize();
  if (!renderer.Initialize()) {
    return 1;
  }

  app.Run(
      [&simulation](float delta_seconds) {
        simulation.Update(delta_seconds);
      },
      [&simulation, &renderer](int framebuffer_width,
                               int framebuffer_height) {
        renderer.Render(simulation.GetRegistry(), framebuffer_width,
                        framebuffer_height);
      });
  return 0;
}
