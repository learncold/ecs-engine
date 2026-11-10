# AGENTS.md

## Research and Documentation

- Preserve the current thesis title while the team reviews a change; do not suggest or apply a title change unless explicitly requested.
- Read [docs/research-goals.md](docs/research-goals.md) before changing simulation,
  ECS storage or benchmarking. Sections 3–6 define the research requirements;
  section 8 defines validation and completion criteria.
- Keep research decisions, experiment conditions, measurement definitions and
  thesis criteria in that document. Track implementation tasks, status and
  roadmaps and issue ordering in GitHub issues; do not create a duplicate research plan.
- [docs/architecture.md](docs/architecture.md) describes the implemented design;
  update it when module boundaries, storage or runtime flow change. Research
  requirements do not imply that a feature is already implemented.
- Update [docs/tech-stack.md](docs/tech-stack.md) when build tools, dependencies,
  graphics assumptions or runtime scope change. Keep [README.md](README.md) concise.
- Preserve `docs/제출용_붙임2_계획서.pdf` and `docs/제출용_붙임3_신청서.pdf`
  unless explicitly asked to edit them.

## Scope

Build a C++20 lightweight runtime for single-threaded headless 3D Boids research,
comparing OOP, Sparse Set ECS and Archetype ECS. Keep lifecycle, fixed timestep,
World/ECS management and time/memory measurement focused on that simulation.
Rendering extensions, common visualization, camera/UI work and FPS/frame/render
CPU or GPU measurements are outside project and thesis completion. Preserve
existing graphics functionality and dependencies. Avoid unrelated engine features.

## Module Boundaries

- `Engine/`: reusable runtime, ECS, CPU state data and benchmark support;
  keep Boids behavior in `Boids/`.
- `Boids/Common/`: header-only `Boids::Common`, shared configuration and
  `boids::BoidParameters`, without Engine dependencies.
- `Boids/ECS/`: ECS simulation components and systems. `Boids/OOP/` must remain
  independent of `Engine/ECS`.
- `App/`: options, implementation selection, callbacks, output and App-specific
  UI. Do not implement ECS queries, Boids systems, model matrices or OpenGL
  rendering here.
- Keep Transform, MeshRenderer, MeshKind and transform functions in
  `Engine/Renderer/`, under `engine::renderer`, without a separate Scene module.
  CPU transform code belongs to the GLM-only `Engine::RenderData` target;
  simulation code must not acquire OpenGL dependencies.
- In Engine, Boids and App, keep related `.h` and `.cpp` files together in feature
  directories, without mirrored `include/` and `src/` trees. Use the project root
  include path and module-qualified includes; preserve target dependency boundaries.
  Tests and third-party code retain their separate layouts.

## Simulation and Storage Rules

These are implementation requirements for the research comparison.

- Use lightweight Entity IDs, plain component data where possible and behavior in systems.
  Prefer contiguous ECS component storage. Support the defined Boid/Dummy
  combinations and entity creation/deletion; avoid a general-purpose migration engine.
- OOP must derive BoidObject and DummyObject from SimulationObject, which holds
  Transform and Velocity and has a virtual destructor. Only BoidObject adds
  Acceleration and BoidParameters. Own both types in one
  `std::vector<std::unique_ptr<SimulationObject>>`.
- Dummy has Transform and Velocity without BoidParameters. Exclude Dummy from
  all Boids calculations, including neighbor search and state updates.
- Use the same seed, initial state, parameters, creation order and fixed timestep
  across implementations. Initially alternate Boid and Dummy creation, then append
  any remaining entities. Use the same naive all-pairs neighbor search.
- Each step deletes/recreates `floor(dummy_count * n / 100)` Dummies, with runtime
  `n` in `[0, 100]`; `n = 0` means no changes. Prepare the same logical deletion
  plan from a fixed seed before measurement. Preserve Boid and final Dummy counts.
- Step order: entity changes → query preparation → neighbor calculation → state
  update. Calculate all accelerations from pre-update positions/velocities before
  changing state. State update applies acceleration, speed limiting and position
  integration, boundary handling, then orientation from final velocity.
- Sparse Set neighbor queries reuse CachedView entity/dense indices; refresh after
  relevant structural changes and do not mutate storage during iteration. Archetype
  queries iterate matching columns/rows. Add only query preparation actually needed.
- `max_alignment_force` limits alignment steering before weighting, not total acceleration.

## Benchmark Rules

- Use VS2022 Release with the same compiler/optimization settings. Run headlessly
  on one thread without graphics initialization; do not run benchmark processes concurrently.
- Measure overall step time and its four intervals in the same run. Overall time
  includes entity changes through state update and instrumentation overhead; do
  not assume the interval sum equals overall time. Exclude initialization, warmup,
  output and external profiling from measured steps.
- Record updates/s, initialization time, Peak Private Commit / Peak Working Set and container
  size/capacity storage estimates. Include auxiliary storage and OOP's pointer
  array plus separately allocated objects without double-counting object fields.
  Query process peaks after timing ends. Distinguish process-wide peaks from
  container estimates and missing values from zero.
- Preserve raw results with execution settings and CPU/OS/compiler/build metadata.
  Entity counts and repetitions follow the research document; they are not fixed here.
  Validate equivalent simulation states within tolerance across implementations.
- Run external cache profiling separately from ordinary time/memory measurement.
  Use section 6 of the research document for metric definitions and interpretation.

## Build and Dependencies

Use the Visual Studio 2022 preset for default verification and before committing
code changes:

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

Executable: `build/vs2022/App/Debug/App.exe`. Run checks appropriate to the change.
After moving source/header files or changing CMake source lists, include paths or
dependencies, run `cmake --preset clangd` to refresh
`build/clangd/compile_commands.json`.

Manage external packages through `vcpkg.json` and `VCPKG_ROOT`. Keep vendored GLAD
in `third_party/glad/`; do not add another OpenGL loader. If regenerating GLAD,
document the OpenGL API/profile. Add dependencies only when necessary for research
or when they materially reduce implementation risk.

## Coding and Git Rules

- Keep changes focused, ownership/lifetimes explicit, headers minimal and classes/
  systems small. Use const where appropriate; avoid unnecessary inheritance and
  global mutable state without an engine-level reason.
- Keep `build/`, `.vs/` and `out/` ignored; do not commit generated build output.
- Check recent relevant commits and merged PRs before committing. Use concise
  English Conventional Commit subjects, such as `feat: add simulation benchmark`.
  Do not include `codex` in commit messages or anywhere in branch names.
- For a single-commit feature, use its commit subject as the PR title.
- When completing a GitHub issue, ignore the PR template and use only
  `Closes #<issue-number>` as the PR body unless the user requests more detail.
