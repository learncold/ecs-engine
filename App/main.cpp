#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

#include "App/UI/performance_panel.h"
#include "App/command_line_options.h"
#include "Boids/Common/boid_simulation_config.h"
#include "Boids/ECS/boid_simulation.h"
#include "Boids/OOP/boid_simulation.h"
#include "Engine/Core/application.h"
#include "Engine/ECSRenderer/ecs_render_system.h"
#include "Engine/Renderer/agent_render_manager.h"

int main(int argc, char* argv[]) try {
  std::vector<std::string_view> arguments;
  for (int index = 1; index < argc; ++index) {
    arguments.emplace_back(argv[index]);
  }
  const auto options = app::ParseCommandLineOptions(arguments);
  if (options.help) {
    app::PrintUsage(std::cout);
    return 0;
  }

  if (options.benchmark_enabled) {
    engine::benchmark::BenchmarkResult result;

    switch (options.implementation) {
      case app::SimulationImplementation::Ecs:
        result = app::RunBenchmark<boids::ecs::BoidSimulation>(options);
        break;
      case app::SimulationImplementation::Oop:
        result = app::RunBenchmark<boids::oop::BoidSimulation>(options);
        break;
    }

    app::PrintBenchmarkResult(std::cout, options, result);
    return 0;
  }

  engine::core::Application application;
  const boids::BoidSimulationConfig simulation_config;
  boids::ecs::BoidSimulation ecs_simulation(simulation_config);
  engine::renderer::AgentRenderManager render_manager(
      simulation_config.boundary_half_extent);
  engine::ecs_renderer::EcsRenderSystem ecs_render_system;
  app::ui::PerformancePanel performance_panel;

  if (!application.Initialize()) {
    return 1;
  }

  ecs_simulation.Initialize();
  if (!render_manager.Initialize()) {
    return 1;
  }

  application.Run(
      [&ecs_simulation, &render_manager](
          float delta_seconds, const engine::input::InputState& input_state) {
        ecs_simulation.Update(delta_seconds);
        render_manager.UpdateCamera(input_state, delta_seconds);
      },
      [&ecs_simulation, &render_manager, &ecs_render_system](
          int framebuffer_width, int framebuffer_height) {
        ecs_render_system.Render(ecs_simulation.GetRegistry(), render_manager,
                                 framebuffer_width, framebuffer_height);
      },
      [&application, &performance_panel,
       boid_count = simulation_config.boid_count]() {
        bool vsync_enabled = application.IsVSyncEnabled();
        if (performance_panel.Draw(boid_count, vsync_enabled)) {
          application.SetVSyncEnabled(vsync_enabled);
        }
      });
  return 0;
} catch (const std::exception& error) {
  std::cerr << "Error: " << error.what() << '\n';
  return 1;
}
