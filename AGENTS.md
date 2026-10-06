# AGENTS.md

## Implementation Checklist

Use [GitHub issue #30](https://github.com/learncold/ecs-engine/issues/30) as the progress checklist for completing the project. Follow its ordered issues and verify each issue's completion criteria before marking it done.

## Project Overview

This project is a graduation research project for building a C++ ECS-based lightweight game engine prototype and evaluating it through a 3D Boids swarm intelligence simulation.

The project is not trying to become a general-purpose 3D game engine like Unity or Unreal Engine. In this repository, "game engine" means a minimal real-time runtime layer that supports:

- Application and game loop
- Window and input handling
- Time step management
- Scene or World management
- ECS-based object management
- Rendering
- Simulation update
- Debug and benchmark measurement

The 3D Boids simulation is the research demo that runs on top of this runtime.

## Research Focus

Prioritize the submitted graduation project direction:

- ECS architecture
- Data-Oriented Design
- 3D Boids swarm simulation
- OOP vs Sparse Set ECS vs Archetype ECS performance comparison
- Single-threaded storage-layout and query-cost analysis
- Quantitative performance measurement

Do not shift the project toward unrelated gameplay features, asset tooling, or a broad commercial engine feature set unless they directly support the research goal.

## Tech Stack

- Language: C++20
- Graphics API: OpenGL 3.3 Core Profile
- Window/Input: GLFW
- OpenGL Loader: GLAD
- Build System: CMake
- Dependency Management: vcpkg + vendored GLAD

## Build Commands

Use the Visual Studio 2022 preset by default.

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

The application executable is generated at:

```text
build/vs2022/App/Debug/App.exe
```

Visual Studio 2026 presets are available, but `vs2022` is the default verification path unless there is a specific reason to use another preset.

The editor uses clangd with `build/clangd/compile_commands.json`, separately from the VS2022 build. After moving source/header files or changing CMake source lists, include paths, or dependencies, run `cmake --preset clangd` as well to refresh editor diagnostics.

## Project Structure

```text
ECS-engine/
├─ AGENTS.md
├─ CMakeLists.txt
├─ CMakePresets.json
├─ README.md
├─ vcpkg.json
├─ docs/
│  ├─ research-goals.md
│  ├─ tech-stack.md
│  ├─ architecture.md
│  ├─ 제출용_붙임2_계획서.pdf
│  └─ 제출용_붙임3_신청서.pdf
├─ Engine/
│  ├─ CMakeLists.txt
│  ├─ Core/
│  ├─ ECS/
│  ├─ ECSRenderer/
│  ├─ Renderer/
│  ├─ Input/
│  └─ DebugUI/
├─ Boids/
│  ├─ CMakeLists.txt
│  ├─ Common/
│  ├─ ECS/
│  └─ OOP/
├─ App/
│  ├─ CMakeLists.txt
│  ├─ main.cpp
│  └─ UI/
└─ third_party/
   └─ glad/
```

## Directory Responsibilities

`Engine/` contains reusable runtime code. Put ECS infrastructure, renderer code, input handling, time management, scene/world code, and benchmark support here. Keep research-demo-specific Boids behavior out of this directory.

`Boids/` contains the research domain and its OOP/ECS implementations. The OOP implementation must not depend on `Engine/ECS`; only the ECS implementation uses that module. Shared initial conditions and parameters may be placed in the common Boids namespace.

`Boids/Common/` provides shared configuration and `boids::BoidParameters` through the header-only `Boids::Common` target, without Engine or ECS dependencies. `Boids/ECS/` contains the ECS implementation; `Boids/OOP/` contains the object-oriented comparison implementation and must remain independent of `Engine/ECS`. `max_alignment_force` limits alignment steering before weighting, not total acceleration.

In `Engine/`, `Boids/`, and `App/`, keep related `.h` and `.cpp` files together in feature directories; do not add mirrored `include/` and `src/` trees. CMake targets use the project root as an include search path to preserve module-qualified includes such as `Engine/Core/application.h` and `Boids/Common/boid_parameters.h`. Keep the existing module dependency boundaries even though headers are visible through this shared search path. Tests and third-party code retain their separate layouts.

`App/` is the thin executable composition layer. It may connect Engine and Boids callbacks and own App-specific UI, but must not implement ECS queries, model-matrix construction, OpenGL rendering, or Boids systems.

`third_party/glad/` contains vendored GLAD source generated for OpenGL loading. Do not replace it casually. If GLAD is regenerated, document the OpenGL API/profile used.

`build/` is generated output and must not be committed.

## Docs Folder

`docs/research-goals.md` defines the research problem, scope, research questions, experiment design, performance metrics, priorities, and exclusions. Update this file when the academic direction changes.

`docs/tech-stack.md` explains the implementation stack, dependency policy, engine scope, and rendering scope. Update this file when build tools, dependencies, graphics API assumptions, or runtime scope change.

`docs/architecture.md` gives a concise overview of module responsibilities, runtime flow, ECS data organization, and the OOP/ECS comparison boundary. Keep it aligned with the implemented design and avoid speculative layers that are not needed by the research prototype.

`docs/제출용_붙임2_계획서.pdf` and `docs/제출용_붙임3_신청서.pdf` are submitted graduation project documents. Treat them as reference material for the original approved topic. Do not edit or replace them unless explicitly requested.

Future recommended docs, only when their topics need more detail:

- `docs/ecs-design.md` for ECS storage and API decisions
- `docs/benchmark-plan.md` for measurement procedures

## Architecture Direction

Keep the project centered on a lightweight runtime, not a full editor or asset pipeline. The target architecture should evolve toward:

```text
Engine.Core
Engine.ECS
Engine.ECSRenderer
Engine.Renderer
Engine.Input
Engine.Benchmark
```

The runtime should support the research demo with the least unnecessary surface area. Boids-specific simulation code belongs in `Boids`, not in a general engine simulation module.

Transform, MeshRenderer, MeshKind, and transform functions live in `Engine/Renderer/` under `engine::renderer`; there is no separate Scene folder or namespace. Keep CPU-only transform code in the GLM-only `Engine::RenderData` CMake target, used by Boids and Renderer, so simulation code does not acquire OpenGL dependencies.

## ECS Design Rules

- Entity should be a lightweight ID.
- Components should be plain data where possible.
- Systems should contain behavior and operate on component data.
- Avoid putting game logic directly into entity classes.
- Prefer contiguous component storage for simulation-critical data.
- Keep the first ECS implementation simple and measurable before optimizing it.
- Do not introduce archetype complexity until the simpler storage strategy is implemented and benchmarked.

Initial ECS implementation may use Sparse Set or type-based component arrays. Archetype ECS is an extension goal, not the first milestone.

## Simulation Rules

The core research simulation is 3D Boids.

Required simulation data should include:

- 3D position
- 3D velocity
- Boid parameters
- Neighbor search radius
- Separation, Alignment, and Cohesion weights

Prefer deterministic setup for benchmark scenarios. Random initialization should use explicit seeds when performance data will be collected.

## Rendering Scope

Rendering exists to support real-time visualization and performance experiments.

Prioritize:

- Basic camera
- Simple 3D agent visualization
- Low-poly triangle or cone-like agent mesh
- Instanced Rendering for many agents
- Stable frame timing

Do not prioritize:

- General-purpose 3D model loading
- PBR
- Advanced lighting and shadows
- Skeletal animation
- Scene editor
- Commercial-engine asset pipeline

## Benchmarking Rules

Performance experiments should measure at least:

- FPS
- Frame time
- Simulation update time
- Rendering time
- Memory usage if practical

Agent counts should be tested in fixed steps, for example:

```text
100
500
1000
3000
5000
10000
```

Benchmark comparisons should separate simulation cost from rendering cost where possible.

Required comparison paths:

- OOP Boids implementation
- Sparse Set ECS Boids implementation
- Archetype ECS Boids implementation
- Naive all-pairs neighbor search

## Coding Style

- Use C++20.
- Prefer clear ownership and explicit lifetimes.
- Avoid unnecessary inheritance.
- Avoid global mutable state unless there is a clear engine-level reason.
- Keep headers minimal.
- Keep dependencies limited.
- Use `const` where appropriate.
- Prefer small, focused classes and systems.
- Keep code changes scoped to the feature or experiment being implemented.

## Dependency Rules

GLFW is managed through `vcpkg.json`.

GLAD is vendored under `third_party/glad` because it is generated for the selected OpenGL API/profile. Do not add another OpenGL loader.

Avoid adding new dependencies unless they materially reduce implementation risk or are necessary for the research goal.

## Git Rules

Do not commit generated build output.

Follow the repository's recent commit and pull request convention:

- Write concise English Conventional Commit subjects such as `feat: add simulation benchmark`. Do not include `codex` in commit messages.
- Use the commit subject as the pull request title when one commit represents the feature.
- For work that completes a GitHub issue, ignore the pull request template and use only `Closes #<issue-number>` as the pull request body unless the user requests additional detail.
- Check the latest relevant commits and merged pull requests before committing so the wording remains consistent with current repository history.

Ignored paths should include:

```text
build/
.vs/
out/
```

Before committing code changes, verify the default build path:

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

## Current Priority

The next implementation priority is integrating the Archetype Boids experiment,
then adding reproducible benchmark runs and result analysis for OOP, Sparse Set
ECS, and Archetype ECS.
