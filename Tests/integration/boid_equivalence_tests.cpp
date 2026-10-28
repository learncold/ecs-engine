#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "Boids/Archetype/boid_simulation.h"
#include "Boids/Common/boid_simulation_config.h"
#include "Boids/Common/simulation_input.h"
#include "Boids/ECS/Components/acceleration.h"
#include "Boids/ECS/Components/velocity.h"
#include "Boids/ECS/boid_simulation.h"
#include "Boids/OOP/boid_object.h"
#include "Boids/OOP/boid_simulation.h"
#include "Boids/OOP/dummy_object.h"
#include "Engine/Measurement/timing_recorder.h"
#include "Engine/Renderer/transform.h"

namespace {

// Absolute tolerance for every position, velocity, acceleration, scale and
// sign-aligned quaternion component. q and -q represent the same rotation;
// both quaternions must also have unit length within this tolerance.
constexpr float kTolerance = 0.00001F;
using boids::ecs::Acceleration;
using boids::ecs::Velocity;
using engine::ecs::Entity;
using engine::measurement::TimingRecorder;
using engine::renderer::Transform;

void ExpectVec3Near(const glm::vec3& actual, const glm::vec3& expected) {
  for (int axis = 0; axis < 3; ++axis) {
    EXPECT_NEAR(actual[axis], expected[axis], kTolerance) << "axis=" << axis;
  }
}

void ExpectRotationNear(const glm::quat& actual, const glm::quat& expected) {
  EXPECT_NEAR(glm::dot(actual, actual), 1.0F, kTolerance);
  EXPECT_NEAR(glm::dot(expected, expected), 1.0F, kTolerance);
  const glm::quat aligned =
      glm::dot(actual, expected) < 0.0F ? -actual : actual;
  for (int component = 0; component < 4; ++component) {
    EXPECT_NEAR(aligned[component], expected[component], kTolerance)
        << "quaternion component=" << component;
  }
}

struct ObjectState {
  boids::ObjectKind kind;
  Transform transform;
  glm::vec3 velocity;
  // Absence checks that Dummy has no Boid acceleration data.
  std::optional<glm::vec3> acceleration;
};

using States = std::vector<ObjectState>;

void ExpectStateNear(const ObjectState& actual, const ObjectState& expected) {
  EXPECT_EQ(actual.kind, expected.kind);
  {
    SCOPED_TRACE("position");
    ExpectVec3Near(actual.transform.position, expected.transform.position);
  }
  {
    SCOPED_TRACE("velocity");
    ExpectVec3Near(actual.velocity, expected.velocity);
  }
  {
    SCOPED_TRACE("acceleration");
    ASSERT_EQ(actual.acceleration.has_value(),
              expected.acceleration.has_value());
    if (actual.acceleration) {
      ExpectVec3Near(*actual.acceleration, *expected.acceleration);
    }
  }
  {
    SCOPED_TRACE("rotation");
    ExpectRotationNear(actual.transform.rotation, expected.transform.rotation);
  }
  {
    SCOPED_TRACE("scale");
    EXPECT_NEAR(actual.transform.scale, expected.transform.scale, kTolerance);
  }
}

// Snapshots are indexed exclusively by logical ID. Physical object order,
// Entity IDs and archetype row order are deliberately not compared.
void ReadStates(const boids::oop::BoidSimulation& simulation, States& states) {
  ASSERT_EQ(simulation.Objects().size(), states.size());
  std::vector<bool> seen(states.size(), false);
  for (const auto& object : simulation.Objects()) {
    ASSERT_NE(object, nullptr);
    const auto id = object->LogicalId();
    SCOPED_TRACE(::testing::Message() << "logical_id=" << id);
    ASSERT_LT(id, states.size());
    ASSERT_FALSE(seen[id]);
    seen[id] = true;
    auto& state = states[id];
    state = {object->Kind(), object->GetTransform(), object->Velocity(), {}};
    if (state.kind == boids::ObjectKind::Boid) {
      const auto* boid =
          dynamic_cast<const boids::oop::BoidObject*>(object.get());
      ASSERT_NE(boid, nullptr);
      state.acceleration = boid->Acceleration();
    } else {
      ASSERT_EQ(state.kind, boids::ObjectKind::Dummy);
      ASSERT_NE(dynamic_cast<const boids::oop::DummyObject*>(object.get()),
                nullptr);
    }
  }
}

void ReadStates(const boids::ecs::BoidSimulation& simulation, States& states) {
  const auto& registry = simulation.GetRegistry();
  ASSERT_EQ(registry.EntityCount(), states.size());
  std::vector<Entity> seen;
  for (std::size_t id = 0; id < states.size(); ++id) {
    SCOPED_TRACE(::testing::Message() << "logical_id=" << id);
    const auto entity = simulation.EntityFor(id);
    ASSERT_TRUE(registry.IsAlive(entity));
    ASSERT_EQ(std::find(seen.begin(), seen.end(), entity), seen.end());
    seen.push_back(entity);
    const auto* transform = registry.FindComponent<Transform>(entity);
    const auto* velocity = registry.FindComponent<Velocity>(entity);
    ASSERT_NE(transform, nullptr);
    ASSERT_NE(velocity, nullptr);
    const bool boid = registry.Has<boids::BoidParameters>(entity);
    auto& state = states[id];
    state = {boid ? boids::ObjectKind::Boid : boids::ObjectKind::Dummy,
             *transform,
             velocity->value,
             {}};
    const auto* acceleration = registry.FindComponent<Acceleration>(entity);
    ASSERT_EQ(acceleration != nullptr, boid);
    if (acceleration) state.acceleration = acceleration->value;
  }
}

void ReadStates(const boids::archetype::BoidSimulation& simulation,
                States& states) {
  const auto& registry = simulation.GetRegistry();
  ASSERT_EQ(registry.EntityCount(), states.size());
  const auto* boids = registry.FindTable<Transform, Velocity, Acceleration,
                                         boids::BoidParameters>();
  const auto* dummies = registry.FindTable<Transform, Velocity>();
  ASSERT_NE(boids, nullptr);
  ASSERT_NE(dummies, nullptr);
  ASSERT_EQ(boids->Size() + dummies->Size(), states.size());
  std::vector<Entity> seen;
  for (std::size_t id = 0; id < states.size(); ++id) {
    SCOPED_TRACE(::testing::Message() << "logical_id=" << id);
    const auto entity = simulation.EntityFor(id);
    ASSERT_TRUE(registry.IsAlive(entity));
    ASSERT_EQ(std::find(seen.begin(), seen.end(), entity), seen.end());
    seen.push_back(entity);
    const auto boid_entities = boids->Entities();
    const auto dummy_entities = dummies->Entities();
    const auto boid =
        std::find(boid_entities.begin(), boid_entities.end(), entity);
    const auto dummy =
        std::find(dummy_entities.begin(), dummy_entities.end(), entity);
    const bool is_boid = boid != boid_entities.end();
    ASSERT_NE(is_boid, dummy != dummy_entities.end());
    const auto* table = is_boid ? boids : dummies;
    const auto row =
        static_cast<std::size_t>(is_boid ? boid - boid_entities.begin()
                                         : dummy - dummy_entities.begin());
    auto& state = states[id];
    state = {is_boid ? boids::ObjectKind::Boid : boids::ObjectKind::Dummy,
             table->Column<Transform>()[row],
             table->Column<Velocity>()[row].value,
             {}};
    if (is_boid) state.acceleration = table->Column<Acceleration>()[row].value;
  }
}

template <typename Simulation>
void UpdateAndCheckEntities(Simulation& simulation, float dt,
                            const boids::SimulationInput& input,
                            std::size_t step, TimingRecorder& timing,
                            bool measured) {
  std::vector<Entity> before;
  for (const auto& initial : input.initial_objects) {
    before.push_back(simulation.EntityFor(initial.logical_id));
  }
  if (measured) timing.begin_step_measurement();
  simulation.Update(dt, input.dummy_steps[step], timing);
  if (measured) timing.end_step_measurement();
  const auto& plan = input.dummy_steps[step];
  for (const auto& initial : input.initial_objects) {
    const auto id = initial.logical_id;
    SCOPED_TRACE(::testing::Message() << "logical_id=" << id);
    const auto current = simulation.EntityFor(id);
    if (std::binary_search(plan.begin(), plan.end(), id)) {
      EXPECT_EQ(initial.kind, boids::ObjectKind::Dummy);
      EXPECT_NE(current, before[id]);
      EXPECT_FALSE(simulation.GetRegistry().IsAlive(before[id]));
    } else {
      EXPECT_EQ(current, before[id]);
    }
  }
}

class ThreeSimulations {
 public:
  explicit ThreeSimulations(const boids::BoidSimulationConfig& config)
      : oop(config), sparse(config), archetype(config) {
    for (auto& timing : timings) timing.set_enabled(false);
  }

  void Initialize(const boids::SimulationInput& input) {
    oop.Initialize(input);
    sparse.Initialize(input);
    archetype.Initialize(input);
    ASSERT_NO_FATAL_FAILURE(CheckStates(input, true));
  }

  void BeginMeasurement(std::size_t steps) {
    for (auto& timing : timings) timing.begin_measurement(steps);
  }

  void Step(const boids::SimulationInput& input, std::size_t step, float dt,
            bool measured) {
    {
      SCOPED_TRACE("OOP");
      if (measured) timings[0].begin_step_measurement();
      oop.Update(dt, input.dummy_steps[step], timings[0]);
      if (measured) timings[0].end_step_measurement();
    }
    {
      SCOPED_TRACE("Sparse Set lifecycle");
      ASSERT_NO_FATAL_FAILURE(UpdateAndCheckEntities(sparse, dt, input, step,
                                                     timings[1], measured));
    }
    {
      SCOPED_TRACE("Archetype lifecycle");
      ASSERT_NO_FATAL_FAILURE(UpdateAndCheckEntities(archetype, dt, input, step,
                                                     timings[2], measured));
    }
    ASSERT_NO_FATAL_FAILURE(CheckStates(input, false));
  }

  void CheckMeasurement(std::size_t steps) const {
    for (std::size_t implementation = 0; implementation < timings.size();
         ++implementation) {
      SCOPED_TRACE(::testing::Message() << "implementation=" << implementation);
      const auto result = timings[implementation].result();
      EXPECT_EQ(result.measured_steps, steps);
      ASSERT_EQ(result.sections.size(), 4U);
      for (const auto& section : result.sections) {
        SCOPED_TRACE(section.name);
        EXPECT_EQ(section.recorded_steps, steps);
      }
    }
  }

  // Public snapshots let metamorphic tests compare all three implementations
  // against a Boid-only run and against inputs with different Dummy data.
  std::array<States, 3> states;

 private:
  void CheckStates(const boids::SimulationInput& input, bool initial_state) {
    for (auto& snapshot : states) snapshot.resize(input.initial_objects.size());
    {
      SCOPED_TRACE("OOP snapshot");
      ASSERT_NO_FATAL_FAILURE(ReadStates(oop, states[0]));
    }
    {
      SCOPED_TRACE("Sparse Set snapshot");
      ASSERT_NO_FATAL_FAILURE(ReadStates(sparse, states[1]));
    }
    {
      SCOPED_TRACE("Archetype snapshot");
      ASSERT_NO_FATAL_FAILURE(ReadStates(archetype, states[2]));
    }
    for (const auto& initial : input.initial_objects) {
      const auto id = initial.logical_id;
      SCOPED_TRACE(::testing::Message() << "logical_id=" << id);
      for (std::size_t implementation = 0; implementation < states.size();
           ++implementation) {
        SCOPED_TRACE(::testing::Message()
                     << "implementation=" << implementation
                     << " (0=OOP, 1=Sparse Set, 2=Archetype)");
        const auto& actual = states[implementation][id];
        EXPECT_EQ(actual.kind, initial.kind);
        if (initial_state || initial.kind == boids::ObjectKind::Dummy) {
          const ObjectState expected{
              initial.kind,
              {.position = initial.position,
               .rotation =
                   engine::renderer::RotationFromDirection(initial.velocity),
               .scale = 0.7F},
              initial.velocity,
              initial.kind == boids::ObjectKind::Boid
                  ? std::optional<glm::vec3>{glm::vec3{0.0F}}
                  : std::nullopt};
          ExpectStateNear(actual, expected);
        } else if (implementation != 0U) {
          ExpectStateNear(actual, states[0][id]);
        }
      }
    }
  }

  boids::oop::BoidSimulation oop;
  boids::ecs::BoidSimulation sparse;
  boids::archetype::BoidSimulation archetype;
  std::array<TimingRecorder, 3> timings{
      TimingRecorder{"entity_changes", "query_preparation",
                     "neighbor_calculation", "state_update"},
      TimingRecorder{"entity_changes", "query_preparation",
                     "neighbor_calculation", "state_update"},
      TimingRecorder{"entity_changes", "query_preparation",
                     "neighbor_calculation", "state_update"}};
};

void CompareSimulations(const boids::BoidSimulationConfig& config,
                        float dt = 1.0F / 60.0F, std::size_t warmup = 7U,
                        std::size_t measured = 80U) {
  SCOPED_TRACE(::testing::Message()
               << "seed=" << config.random_seed << ", boids="
               << config.boid_count << ", dummies=" << config.dummy_count
               << ", percent=" << config.dummy_change_percent << ", dt=" << dt
               << ", warmup=" << warmup << ", measured=" << measured);
  const auto input = boids::PrepareSimulationInput(config, warmup + measured);
  ThreeSimulations simulations(config);
  {
    SCOPED_TRACE("initialization");
    ASSERT_NO_FATAL_FAILURE(simulations.Initialize(input));
  }
  for (std::size_t step = 0; step < input.dummy_steps.size(); ++step) {
    SCOPED_TRACE(::testing::Message()
                 << "plan_step=" << step
                 << (step < warmup ? " warmup" : " measured"));
    if (step == warmup) simulations.BeginMeasurement(measured);
    // Never restart the simulation or plan at the measurement boundary.
    ASSERT_NO_FATAL_FAILURE(simulations.Step(input, step, dt, step >= warmup));
    ASSERT_FALSE(::testing::Test::HasFailure());
  }
  simulations.CheckMeasurement(measured);
}

struct Counts {
  std::size_t boids;
  std::size_t dummies;
};
using Scenario = std::tuple<std::uint32_t, Counts, std::uint32_t>;
class ThreeWayEquivalenceTest : public ::testing::TestWithParam<Scenario> {};

std::string ScenarioName(const ::testing::TestParamInfo<Scenario>& info) {
  const auto [seed, counts, percent] = info.param;
  return "Seed" + std::to_string(seed) + "_Boids" +
         std::to_string(counts.boids) + "_Dummies" +
         std::to_string(counts.dummies) + "_Percent" + std::to_string(percent);
}

TEST_P(ThreeWayEquivalenceTest, MatchesInitializationAndEveryStep) {
  const auto [seed, counts, percent] = GetParam();
  boids::BoidSimulationConfig config;
  config.random_seed = seed;
  config.boid_count = counts.boids;
  config.dummy_count = counts.dummies;
  config.dummy_change_percent = percent;
  config.spawn_half_extent = 2.0F;
  config.boundary_half_extent = 3.0F;
  CompareSimulations(config);
}

INSTANTIATE_TEST_SUITE_P(
    SeedsCountsAndChurn, ThreeWayEquivalenceTest,
    ::testing::Combine(::testing::Values(0U, 42U, 12345U),
                       ::testing::Values(Counts{17U, 23U}, Counts{23U, 7U},
                                         Counts{9U, 0U}, Counts{0U, 9U},
                                         Counts{0U, 0U}, Counts{1U, 3U},
                                         Counts{4U, 2U}),
                       ::testing::Values(0U, 40U, 100U)),
    ScenarioName);

TEST(BoidEquivalenceTest,
     ThreeImplementationsMatchAcrossLongWarmupAndMeasurement) {
  for (const std::uint32_t seed : {0U, 42U, 12345U}) {
    for (const std::uint32_t percent : {0U, 40U, 100U}) {
      boids::BoidSimulationConfig config;
      config.boid_count = 17U;
      config.dummy_count = 23U;
      config.random_seed = seed;
      config.dummy_change_percent = percent;
      config.spawn_half_extent = 2.0F;
      config.boundary_half_extent = 3.0F;
      ASSERT_NO_FATAL_FAILURE(
          CompareSimulations(config, 1.0F / 60.0F, 100U, 1000U));
    }
  }
}

TEST(BoidEquivalenceTest, MatchesWithSpeedLimitingAndBoundaryReflections) {
  boids::BoidSimulationConfig config;
  config.boid_count = 12U;
  config.dummy_count = 7U;
  config.dummy_change_percent = 40U;
  config.spawn_half_extent = 1.0F;
  config.boundary_half_extent = 1.0F;
  config.initial_speed = 10.0F;
  config.parameters.max_speed = 3.0F;
  CompareSimulations(config, 0.05F);
}

TEST(BoidEquivalenceTest, MatchesWithoutNeighbors) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.dummy_count = 7U;
  config.dummy_change_percent = 100U;
  config.parameters.neighbor_radius = 0.0F;
  config.parameters.separation_radius = 0.0F;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest, MatchesWithCoincidentPositions) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.dummy_count = 7U;
  config.dummy_change_percent = 100U;
  config.spawn_half_extent = 0.0F;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest, MatchesWithZeroSpeed) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.dummy_count = 7U;
  config.dummy_change_percent = 40U;
  config.initial_speed = 0.0F;
  config.parameters.max_speed = 0.0F;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest, MatchesWithZeroDeltaTime) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.dummy_count = 7U;
  config.dummy_change_percent = 100U;
  CompareSimulations(config, 0.0F);
}

TEST(BoidEquivalenceTest, MatchesWithLargeFiniteInitialSpeed) {
  boids::BoidSimulationConfig config;
  config.boid_count = 8U;
  config.dummy_count = 7U;
  config.dummy_change_percent = 40U;
  config.initial_speed = 1e20F;
  CompareSimulations(config);
}

TEST(BoidEquivalenceTest,
     DummyPresenceDataAndChurnDoNotAffectAnyImplementation) {
  constexpr std::size_t kWarmup = 7U;
  constexpr std::size_t kMeasured = 80U;
  constexpr float kDt = 1.0F / 60.0F;
  for (const std::uint32_t seed : {0U, 42U, 12345U}) {
    for (const std::uint32_t percent : {0U, 40U, 100U}) {
      SCOPED_TRACE(::testing::Message()
                   << "seed=" << seed << ", boids=7, dummies=11, percent="
                   << percent << ", dt=" << kDt << ", warmup=" << kWarmup
                   << ", measured=" << kMeasured);
      boids::BoidSimulationConfig config;
      config.boid_count = 7U;
      config.dummy_count = 11U;
      config.dummy_change_percent = percent;
      config.random_seed = seed;
      config.spawn_half_extent = 2.0F;
      config.boundary_half_extent = 3.0F;
      auto input = boids::PrepareSimulationInput(config, kWarmup + kMeasured);
      auto altered_input = input;
      boids::SimulationInput boid_only_input;
      boid_only_input.dummy_steps.resize(input.dummy_steps.size());
      std::vector<std::size_t> mixed_boid_ids;
      for (auto& initial : input.initial_objects) {
        if (initial.kind == boids::ObjectKind::Boid) {
          mixed_boid_ids.push_back(initial.logical_id);
          auto boid = initial;
          boid.logical_id = boid_only_input.initial_objects.size();
          boid_only_input.initial_objects.push_back(boid);
        } else {
          // Nearby fast Dummies would pollute separation/alignment/cohesion.
          initial.position = input.initial_objects.front().position;
          initial.velocity = {100.0F, -80.0F, 60.0F};
          // Outside-boundary Dummies also detect accidental state updates.
          auto& altered = altered_input.initial_objects[initial.logical_id];
          altered.position = {100.0F, -200.0F, 300.0F};
          altered.velocity = {-70.0F, 90.0F, -110.0F};
        }
      }
      auto boid_only_config = config;
      boid_only_config.dummy_count = 0U;
      boid_only_config.dummy_change_percent = 0U;
      ThreeSimulations mixed(config), altered(config),
          boid_only(boid_only_config);
      ASSERT_NO_FATAL_FAILURE(mixed.Initialize(input));
      ASSERT_NO_FATAL_FAILURE(altered.Initialize(altered_input));
      ASSERT_NO_FATAL_FAILURE(boid_only.Initialize(boid_only_input));
      for (std::size_t step = 0; step <= input.dummy_steps.size(); ++step) {
        SCOPED_TRACE(::testing::Message() << "completed_steps=" << step);
        if (step > 0U) {
          const auto plan_step = step - 1U;
          if (plan_step == kWarmup) {
            mixed.BeginMeasurement(kMeasured);
            altered.BeginMeasurement(kMeasured);
            boid_only.BeginMeasurement(kMeasured);
          }
          const bool measured = plan_step >= kWarmup;
          ASSERT_NO_FATAL_FAILURE(mixed.Step(input, plan_step, kDt, measured));
          ASSERT_NO_FATAL_FAILURE(
              altered.Step(altered_input, plan_step, kDt, measured));
          ASSERT_NO_FATAL_FAILURE(
              boid_only.Step(boid_only_input, plan_step, kDt, measured));
        }
        for (std::size_t implementation = 0; implementation < 3U;
             ++implementation) {
          SCOPED_TRACE(::testing::Message()
                       << "implementation=" << implementation);
          for (std::size_t id = 0; id < mixed_boid_ids.size(); ++id) {
            const auto mixed_id = mixed_boid_ids[id];
            SCOPED_TRACE(::testing::Message()
                         << "mixed logical_id=" << mixed_id
                         << ", boid-only logical_id=" << id);
            const auto& expected = boid_only.states[implementation][id];
            ExpectStateNear(mixed.states[implementation][mixed_id], expected);
            ExpectStateNear(altered.states[implementation][mixed_id], expected);
          }
        }
        ASSERT_FALSE(::testing::Test::HasFailure());
      }
      mixed.CheckMeasurement(kMeasured);
      altered.CheckMeasurement(kMeasured);
      boid_only.CheckMeasurement(kMeasured);
    }
  }
}

// All implementations reject overflow and require reinitialization afterward.
void ExpectArithmeticOverflow(const boids::BoidSimulationConfig& config,
                              const boids::SimulationInput& input, float dt) {
  boids::archetype::BoidSimulation archetype(config);
  archetype.Initialize(input);
  engine::measurement::TimingRecorder timing(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  timing.begin_measurement(1U);
  timing.begin_step_measurement();
  EXPECT_THROW(archetype.Update(dt, {}, timing), std::overflow_error);
  EXPECT_THROW(timing.end_step_measurement(), std::logic_error);
  EXPECT_THROW(static_cast<void>(timing.result()), std::logic_error);
  engine::measurement::TimingRecorder disabled;
  disabled.set_enabled(false);
  EXPECT_THROW(archetype.Update(dt, {}, disabled), std::logic_error);

  boids::oop::BoidSimulation oop(config);
  oop.Initialize(input);
  engine::measurement::TimingRecorder oop_timing(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  oop_timing.begin_measurement(1U);
  oop_timing.begin_step_measurement();
  EXPECT_THROW(oop.Update(dt, {}, oop_timing), std::overflow_error);
  EXPECT_THROW(oop_timing.end_step_measurement(), std::logic_error);
  EXPECT_THROW(static_cast<void>(oop_timing.result()), std::logic_error);
  EXPECT_THROW(oop.Update(dt, {}, disabled), std::logic_error);
  auto reset_input = input;
  for (auto& initial : reset_input.initial_objects) {
    initial.position = glm::vec3{0.0F};
    initial.velocity = glm::vec3{0.0F};
  }
  oop.Initialize(reset_input);
  EXPECT_NO_THROW(oop.Update(0.0F, {}, disabled));

  boids::ecs::BoidSimulation ecs(config);
  ecs.Initialize(input);
  engine::measurement::TimingRecorder ecs_timing(
      {"entity_changes", "query_preparation", "neighbor_calculation",
       "state_update"});
  ecs_timing.begin_measurement(1U);
  ecs_timing.begin_step_measurement();
  EXPECT_THROW(ecs.Update(dt, {}, ecs_timing), std::overflow_error);
  EXPECT_THROW(ecs_timing.end_step_measurement(), std::logic_error);
  EXPECT_THROW(static_cast<void>(ecs_timing.result()), std::logic_error);
  EXPECT_THROW(ecs.Update(dt, {}, disabled), std::logic_error);
  ecs.Initialize(reset_input);
  EXPECT_NO_THROW(ecs.Update(0.0F, {}, disabled));
}

TEST(BoidEquivalenceTest, RejectsPositionOverflowBeforeBoundaryClamping) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  config.boundary_half_extent = 3.4e38F;
  config.parameters.max_speed = 1e38F;
  const boids::SimulationInput input{
      .initial_objects =
          {{boids::ObjectKind::Boid, 0U, {1e38F, 0, 0}, {-1e38F, 0, 0}}},
      .dummy_steps = {}};
  // The exact sum would still be inside the boundary, but the intermediate
  // float product overflows. Reject it instead of inventing a collision.
  ExpectArithmeticOverflow(config, input, 4.0F);
}

TEST(BoidEquivalenceTest, RejectsVelocityOverflowEvenWithZeroSpeedLimit) {
  boids::BoidSimulationConfig config;
  config.boid_count = 2U;
  config.parameters.cohesion_weight = 1e38F;
  config.parameters.max_speed = 0.0F;
  const boids::SimulationInput input{
      .initial_objects = {{boids::ObjectKind::Boid, 0U, {0, 0, 0}, {0, 0, 0}},
                          {boids::ObjectKind::Boid, 1U, {2, 0, 0}, {0, 0, 0}}},
      .dummy_steps = {}};
  ExpectArithmeticOverflow(config, input, 2.0F);
}

TEST(BoidEquivalenceTest, RejectsNeighborOverflowEvenWithZeroAlignmentForce) {
  boids::BoidSimulationConfig config;
  config.boid_count = 3U;
  config.parameters.max_alignment_force = 0.0F;
  config.parameters.separation_weight = 0.0F;
  config.parameters.cohesion_weight = 0.0F;
  const boids::SimulationInput input{
      .initial_objects =
          {{boids::ObjectKind::Boid, 0U, {0, 0, 0}, {2e38F, 0, 0}},
           {boids::ObjectKind::Boid, 1U, {1, 0, 0}, {2e38F, 0, 0}},
           {boids::ObjectKind::Boid, 2U, {2, 0, 0}, {2e38F, 0, 0}}},
      .dummy_steps = {}};
  ExpectArithmeticOverflow(config, input, 0.01F);
}

TEST(BoidEquivalenceTest, LegacySimulationsRejectInvalidConfigAndDt) {
  for (const float invalid : {-1.0F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
    boids::BoidSimulationConfig config;
    config.initial_speed = invalid;
    EXPECT_THROW(static_cast<void>(boids::ecs::BoidSimulation{config}),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(boids::oop::BoidSimulation{config}),
                 std::invalid_argument);
    config = {};
    config.parameters.max_speed = invalid;
    EXPECT_THROW(static_cast<void>(boids::ecs::BoidSimulation{config}),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(boids::oop::BoidSimulation{config}),
                 std::invalid_argument);
    config = {};
    config.boid_count = 1U;
    boids::ecs::BoidSimulation ecs(config);
    boids::oop::BoidSimulation oop(config);
    ecs.Initialize();
    oop.Initialize();
    const auto before = oop.Objects()[0]->Position();
    EXPECT_THROW(ecs.Update(invalid), std::invalid_argument);
    EXPECT_THROW(oop.Update(invalid), std::invalid_argument);
    ExpectVec3Near(oop.Objects()[0]->Position(), before);
    EXPECT_NO_THROW(ecs.Update(0.01F));
    EXPECT_NO_THROW(oop.Update(0.01F));
  }
}

}  // namespace
