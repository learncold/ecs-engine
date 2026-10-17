#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Boids/Common/simulation_input.h"
#include "Boids/OOP/boid_object.h"
#include "Boids/OOP/boid_simulation.h"
#include "Boids/OOP/dummy_object.h"

namespace {

using boids::ObjectKind;
using boids::SimulationInput;
using boids::oop::BoidObject;
using boids::oop::BoidSimulation;
using boids::oop::DummyObject;
using boids::oop::SimulationObject;
using engine::measurement::TimingRecorder;

static_assert(std::has_virtual_destructor_v<SimulationObject>);
static_assert(std::is_base_of_v<SimulationObject, BoidObject>);
static_assert(std::is_base_of_v<SimulationObject, DummyObject>);
static_assert(!std::is_copy_constructible_v<SimulationObject>);
static_assert(!std::is_copy_assignable_v<SimulationObject>);
static_assert(!std::is_move_assignable_v<SimulationObject>);
static_assert(!std::is_copy_assignable_v<BoidObject>);
static_assert(!std::is_copy_assignable_v<DummyObject>);
static_assert(!std::is_copy_constructible_v<BoidSimulation>);

// Direct fixtures deliberately do not depend on Common's input generator.
SimulationInput MakeInput(std::size_t boids, std::size_t dummies) {
  SimulationInput input;
  std::size_t boid = 0U;
  std::size_t dummy = 0U;
  const auto append = [&](ObjectKind kind, std::size_t ordinal) {
    const float value = static_cast<float>(ordinal);
    input.initial_objects.push_back(
        {.kind = kind,
         .logical_id = input.initial_objects.size(),
         .position = kind == ObjectKind::Boid
                         ? glm::vec3{value * 0.7F, 0.2F, 0.3F}
                         : glm::vec3{50.0F + value, 0.0F, 0.0F},
         .velocity = kind == ObjectKind::Boid
                         ? glm::vec3{1.0F, value * 0.1F, 0.3F}
                         : glm::vec3{100.0F, value, 0.0F}});
  };
  while (boid < boids || dummy < dummies) {
    if (boid < boids) append(ObjectKind::Boid, boid++);
    if (dummy < dummies) append(ObjectKind::Dummy, dummy++);
  }
  return input;
}

const SimulationObject* Find(const BoidSimulation& simulation, std::size_t id) {
  for (const auto& object : simulation.Objects()) {
    if (object->LogicalId() == id) return object.get();
  }
  return nullptr;
}

void ExpectVecNear(const glm::vec3& actual, const glm::vec3& expected) {
  for (int axis = 0; axis < 3; ++axis) {
    EXPECT_NEAR(actual[axis], expected[axis], 0.00001F);
  }
}

void ExpectInitialDummies(const BoidSimulation& simulation,
                          const SimulationInput& input) {
  ASSERT_EQ(simulation.Objects().size(), input.initial_objects.size());
  for (const auto& initial : input.initial_objects) {
    const auto* object = Find(simulation, initial.logical_id);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->Kind(), initial.kind);
    if (initial.kind == ObjectKind::Dummy) {
      ExpectVecNear(object->Position(), initial.position);
      ExpectVecNear(object->Velocity(), initial.velocity);
      EXPECT_NEAR(std::abs(glm::dot(object->GetTransform().rotation,
                                    engine::renderer::RotationFromDirection(
                                        initial.velocity))),
                  1.0F, 0.00001F);
      EXPECT_FLOAT_EQ(object->GetTransform().scale, 0.7F);
    }
  }
}

class LifetimeProbe final : public SimulationObject {
 public:
  LifetimeProbe(const boids::InitialObjectState& initial, int& destructions)
      : SimulationObject(initial.kind, initial.logical_id, initial.position,
                         initial.velocity),
        destructions_(destructions) {}
  ~LifetimeProbe() override { ++destructions_; }

 private:
  int& destructions_;
};

TEST(OopSimulationTest, OwnsBothKindsInInputOrderAndInitializesTransforms) {
  boids::BoidSimulationConfig config;
  config.boid_count = 3U;
  config.dummy_count = 2U;
  const auto input = MakeInput(3U, 2U);
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  ASSERT_EQ(simulation.Objects().size(), 5U);
  for (std::size_t index = 0; index < input.initial_objects.size(); ++index) {
    const auto& object = *simulation.Objects()[index];
    const auto& initial = input.initial_objects[index];
    EXPECT_EQ(object.Kind(), initial.kind);
    EXPECT_EQ(object.LogicalId(), index);
    ExpectVecNear(object.Position(), initial.position);
    ExpectVecNear(object.Velocity(), initial.velocity);
    EXPECT_FLOAT_EQ(object.GetTransform().scale, 0.7F);
    EXPECT_NEAR(std::abs(glm::dot(
                    object.GetTransform().rotation,
                    engine::renderer::RotationFromDirection(initial.velocity))),
                1.0F, 0.00001F);
    if (object.Kind() == ObjectKind::Boid) {
      const auto* boid = dynamic_cast<const BoidObject*>(&object);
      ASSERT_NE(boid, nullptr);
      ExpectVecNear(boid->Acceleration(), glm::vec3{0.0F});
    } else {
      EXPECT_NE(dynamic_cast<const DummyObject*>(&object), nullptr);
    }
  }
}

TEST(OopSimulationTest,
     DestroysThroughParentAndReinitializesOwnershipAndIndices) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  config.dummy_count = 2U;
  auto input = MakeInput(1U, 2U);
  int destructions = 0;
  {
    BoidSimulation simulation(config);
    simulation.Initialize(input);
    // The const ownership view is cast only to inject a destructor probe,
    // without adding a test-only mutation API to production code.
    auto& slot =
        const_cast<std::unique_ptr<SimulationObject>&>(simulation.Objects()[1]);
    slot =
        std::make_unique<LifetimeProbe>(input.initial_objects[1], destructions);
    auto replacement = input;
    replacement.initial_objects[0].position.x = -3.0F;
    simulation.Initialize(replacement);
    EXPECT_EQ(destructions, 1);
    ExpectVecNear(Find(simulation, 0U)->Position(),
                  replacement.initial_objects[0].position);
    auto& new_slot =
        const_cast<std::unique_ptr<SimulationObject>&>(simulation.Objects()[2]);
    new_slot = std::make_unique<LifetimeProbe>(replacement.initial_objects[2],
                                               destructions);
  }
  EXPECT_EQ(destructions, 2);
}

TEST(OopSimulationTest, DestroysSelectedDummyAndCreatesItsOriginalStateAgain) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  config.dummy_count = 2U;
  config.dummy_change_percent = 50U;
  const auto input = MakeInput(1U, 2U);
  int destructions = 0;
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  auto& slot =
      const_cast<std::unique_ptr<SimulationObject>&>(simulation.Objects()[1]);
  slot =
      std::make_unique<LifetimeProbe>(input.initial_objects[1], destructions);
  const_cast<glm::vec3&>(slot->Velocity()) = glm::vec3{-8.0F};
  const_cast<engine::renderer::Transform&>(slot->GetTransform()).position =
      glm::vec3{-8.0F};
  const auto* boid = Find(simulation, 0U);
  TimingRecorder disabled;
  disabled.set_enabled(false);
  const std::array<std::size_t, 1> ids{1U};

  simulation.Update(0.0F, ids, disabled);
  EXPECT_EQ(destructions, 1);
  EXPECT_NE(dynamic_cast<const DummyObject*>(Find(simulation, 1U)), nullptr);
  EXPECT_EQ(Find(simulation, 0U), boid);
  ExpectInitialDummies(simulation, input);
}

TEST(OopSimulationTest, RepeatedChangesFollowLogicalIdsAfterSwapAndPop) {
  boids::BoidSimulationConfig config;
  config.boid_count = 3U;
  config.dummy_count = 3U;
  config.dummy_change_percent = 67U;
  const auto input = MakeInput(3U, 3U);
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  const auto* first_boid = Find(simulation, 0U);
  TimingRecorder disabled;
  disabled.set_enabled(false);
  const std::array<std::array<std::size_t, 2>, 3> plans{
      {{1U, 5U}, {3U, 5U}, {1U, 3U}}};
  for (int step = 0; step < 30; ++step) {
    simulation.Update(0.02F, plans[step % plans.size()], disabled);
    ExpectInitialDummies(simulation, input);
    EXPECT_EQ(Find(simulation, 0U), first_boid);
    const auto& plan = plans[step % plans.size()];
    EXPECT_EQ(simulation.Objects()[4]->LogicalId(), plan[0]);
    EXPECT_EQ(simulation.Objects()[5]->LogicalId(), plan[1]);
  }
  simulation.Initialize(input);
  simulation.Update(0.0F, plans[0], disabled);
  ExpectInitialDummies(simulation, input);
}

TEST(OopSimulationTest, DummyStatesAndChurnDoNotAffectBoids) {
  boids::BoidSimulationConfig config;
  config.boid_count = 4U;
  config.dummy_count = 4U;
  config.dummy_change_percent = 100U;
  auto near_input = MakeInput(4U, 4U);
  auto far_input = near_input;
  for (auto& initial : near_input.initial_objects) {
    if (initial.kind == ObjectKind::Dummy) {
      initial.position = glm::vec3{0.0F};
      initial.velocity = glm::vec3{-900.0F};
    }
  }
  BoidSimulation near_simulation(config);
  BoidSimulation far_simulation(config);
  near_simulation.Initialize(near_input);
  far_simulation.Initialize(far_input);
  TimingRecorder disabled;
  disabled.set_enabled(false);
  const std::array<std::size_t, 4> ids{1U, 3U, 5U, 7U};
  for (int step = 0; step < 20; ++step) {
    near_simulation.Update(0.02F, ids, disabled);
    far_simulation.Update(0.02F, ids, disabled);
    for (std::size_t id : {0U, 2U, 4U, 6U}) {
      const auto& near =
          static_cast<const BoidObject&>(*Find(near_simulation, id));
      const auto& far =
          static_cast<const BoidObject&>(*Find(far_simulation, id));
      ExpectVecNear(near.Position(), far.Position());
      ExpectVecNear(near.Velocity(), far.Velocity());
      ExpectVecNear(near.Acceleration(), far.Acceleration());
    }
    ExpectInitialDummies(near_simulation, near_input);
    ExpectInitialDummies(far_simulation, far_input);
  }
}

TEST(OopSimulationTest, ChurnPreservesBoidResultsWithinTolerance) {
  boids::BoidSimulationConfig config;
  config.boid_count = 4U;
  config.dummy_count = 4U;
  config.dummy_change_percent = 100U;
  const auto input = MakeInput(4U, 4U);
  BoidSimulation unchanged(config);
  BoidSimulation changed(config);
  unchanged.Initialize(input);
  changed.Initialize(input);
  TimingRecorder disabled;
  disabled.set_enabled(false);
  const std::array<std::size_t, 4> ids{1U, 3U, 5U, 7U};
  for (int step = 0; step < 30; ++step) {
    unchanged.Update(0.02F, {}, disabled);
    changed.Update(0.02F, ids, disabled);
    for (std::size_t id : {0U, 2U, 4U, 6U}) {
      ExpectVecNear(Find(changed, id)->Position(),
                    Find(unchanged, id)->Position());
      ExpectVecNear(Find(changed, id)->Velocity(),
                    Find(unchanged, id)->Velocity());
    }
  }
}

TEST(OopSimulationTest, CalculatesAllAccelerationsBeforeAnyMovement) {
  boids::BoidSimulationConfig config;
  config.boid_count = 2U;
  config.dummy_count = 1U;
  config.parameters.separation_weight = 0.0F;
  config.parameters.alignment_weight = 0.0F;
  config.parameters.cohesion_weight = 1.0F;
  config.parameters.max_speed = 100.0F;
  auto input = MakeInput(2U, 1U);
  input.initial_objects[0].position = glm::vec3{0.0F};
  input.initial_objects[0].velocity = glm::vec3{0.0F};
  input.initial_objects[2].position = glm::vec3{2.0F, 0.0F, 0.0F};
  input.initial_objects[2].velocity = glm::vec3{0.0F};
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  simulation.Update(0.5F);
  const auto& left = static_cast<const BoidObject&>(*Find(simulation, 0U));
  const auto& right = static_cast<const BoidObject&>(*Find(simulation, 2U));
  ExpectVecNear(left.Acceleration(), glm::vec3{2.0F, 0.0F, 0.0F});
  ExpectVecNear(right.Acceleration(), glm::vec3{-2.0F, 0.0F, 0.0F});
  ExpectVecNear(left.Position(), glm::vec3{0.5F, 0.0F, 0.0F});
  ExpectVecNear(right.Position(), glm::vec3{1.5F, 0.0F, 0.0F});
  ExpectInitialDummies(simulation, input);
}

TEST(OopSimulationTest, LimitsAlignmentBeforeWeighting) {
  boids::BoidSimulationConfig config;
  config.boid_count = 2U;
  config.parameters.separation_weight = 0.0F;
  config.parameters.cohesion_weight = 0.0F;
  config.parameters.alignment_weight = 3.0F;
  config.parameters.max_alignment_force = 0.25F;
  auto input = MakeInput(2U, 0U);
  input.initial_objects[0].velocity = glm::vec3{1.0F, 0.0F, 0.0F};
  input.initial_objects[1].velocity = glm::vec3{0.0F, 1.0F, 0.0F};
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  simulation.Update(0.0F);
  const auto& boid = static_cast<const BoidObject&>(*Find(simulation, 0U));
  EXPECT_NEAR(glm::length(boid.Acceleration()), 0.75F, 0.00001F);
  ExpectVecNear(boid.Position(), input.initial_objects[0].position);
  ExpectVecNear(boid.Velocity(), input.initial_objects[0].velocity);
}

TEST(OopSimulationTest, LimitsSpeedThenReflectsAndOrientsFromFinalVelocity) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  config.boundary_half_extent = 1.0F;
  config.spawn_half_extent = 1.0F;
  config.parameters.max_speed = 3.0F;
  auto input = MakeInput(1U, 0U);
  input.initial_objects[0].position = glm::vec3{0.9F, 0.0F, 0.0F};
  input.initial_objects[0].velocity = glm::vec3{10.0F, 0.0F, 0.0F};
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  simulation.Update(0.1F);
  const auto& boid = *Find(simulation, 0U);
  ExpectVecNear(boid.Position(), glm::vec3{1.0F, 0.0F, 0.0F});
  ExpectVecNear(boid.Velocity(), glm::vec3{-3.0F, 0.0F, 0.0F});
  EXPECT_NEAR(std::abs(glm::dot(
                  boid.GetTransform().rotation,
                  engine::renderer::RotationFromDirection(boid.Velocity()))),
              1.0F, 0.00001F);
}

TEST(OopSimulationTest,
     SupportsEmptyAndDummyOnlySimulationsAndFullReplacement) {
  for (std::size_t count : {0U, 3U}) {
    boids::BoidSimulationConfig config;
    config.boid_count = 0U;
    config.dummy_count = count;
    config.dummy_change_percent = 100U;
    const auto input = MakeInput(0U, count);
    BoidSimulation simulation(config);
    simulation.Initialize(input);
    TimingRecorder disabled;
    disabled.set_enabled(false);
    std::vector<std::size_t> ids;
    for (std::size_t id = 0; id < count; ++id) ids.push_back(id);
    for (int step = 0; step < 3; ++step) {
      simulation.Update(0.02F, ids, disabled);
      ExpectInitialDummies(simulation, input);
    }
  }
}

TEST(OopSimulationTest,
     RejectsUninitializedUpdateAndNonfiniteOrNegativeDeltaTime) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  BoidSimulation simulation(config);
  TimingRecorder disabled;
  disabled.set_enabled(false);
  EXPECT_THROW(simulation.Update(0.02F), std::logic_error);
  EXPECT_THROW(simulation.Update(0.02F, {}, disabled), std::logic_error);
  const auto input = MakeInput(1U, 0U);
  simulation.Initialize(input);
  for (float dt : {-1.0F, std::numeric_limits<float>::infinity(),
                   -std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::quiet_NaN()}) {
    EXPECT_THROW(simulation.Update(dt, {}, disabled), std::invalid_argument);
    EXPECT_THROW(simulation.Update(dt), std::invalid_argument);
  }
  EXPECT_NO_THROW(simulation.Update(0.0F));
  ExpectVecNear(Find(simulation, 0U)->Position(),
                input.initial_objects[0].position);
}

TEST(OopSimulationTest, ValidatesConfigAndOverflowBeforeCreatingObjects) {
  boids::BoidSimulationConfig config;
  config.dummy_change_percent = 101U;
  EXPECT_THROW((BoidSimulation{config}), std::invalid_argument);
  config.dummy_change_percent = 0U;
  config.boid_count = std::numeric_limits<std::size_t>::max();
  config.dummy_count = 1U;
  EXPECT_THROW((BoidSimulation{config}), std::overflow_error);
  config = {};
  config.spawn_half_extent = std::numeric_limits<float>::max();
  config.boundary_half_extent = std::numeric_limits<float>::max();
  EXPECT_THROW((BoidSimulation{config}), std::invalid_argument);
  for (float value : {std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
    config = {};
    config.boundary_half_extent = value;
    EXPECT_THROW((BoidSimulation{config}), std::invalid_argument);
    config = {};
    config.spawn_half_extent = value;
    EXPECT_THROW((BoidSimulation{config}), std::invalid_argument);
    config = {};
    config.initial_speed = value;
    EXPECT_THROW((BoidSimulation{config}), std::invalid_argument);
  }
}

TEST(OopSimulationTest, RejectsInvalidBehaviorParametersWithBorrowedInput) {
  const auto input = MakeInput(1U, 0U);
  using Parameter = float boids::BoidParameters::*;
  const std::array<Parameter, 8> parameters{
      &boids::BoidParameters::neighbor_radius,
      &boids::BoidParameters::separation_radius,
      &boids::BoidParameters::separation_weight,
      &boids::BoidParameters::alignment_weight,
      &boids::BoidParameters::cohesion_weight,
      &boids::BoidParameters::preferred_speed,
      &boids::BoidParameters::max_speed,
      &boids::BoidParameters::max_alignment_force};
  for (const auto parameter : parameters) {
    for (const float value : {-1.0F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
      boids::BoidSimulationConfig config;
      config.boid_count = 1U;
      config.parameters.*parameter = value;
      EXPECT_THROW(
          {
            BoidSimulation simulation(config);
            simulation.Initialize(input);
          },
          std::invalid_argument);
    }
  }
}

TEST(OopSimulationTest, RejectsMismatchedCountsKindsIdsAndInvalidInitialState) {
  boids::BoidSimulationConfig config;
  config.boid_count = 1U;
  config.dummy_count = 1U;
  const auto valid = MakeInput(1U, 1U);
  BoidSimulation simulation(config);
  simulation.Initialize(valid);
  const auto* original = Find(simulation, 0U);
  for (int condition = 0; condition < 6; ++condition) {
    auto invalid = valid;
    switch (condition) {
      case 0:
        invalid.initial_objects.pop_back();
        break;
      case 1:
        invalid.initial_objects[1].kind = ObjectKind::Boid;
        break;
      case 2:
        invalid.initial_objects[1].kind = static_cast<ObjectKind>(99);
        break;
      case 3:
        invalid.initial_objects[1].logical_id = 0U;
        break;
      case 4:
        invalid.initial_objects[0].logical_id = 100U;
        break;
      case 5:
        invalid.initial_objects[1].position.x =
            std::numeric_limits<float>::quiet_NaN();
        break;
    }
    EXPECT_THROW(simulation.Initialize(invalid), std::invalid_argument);
    EXPECT_EQ(Find(simulation, 0U), original);
  }
  EXPECT_NO_THROW(simulation.Update(0.0F));
}

TEST(OopSimulationTest, PlanlessUpdateRequiresActualZeroChanges) {
  for (std::uint32_t percent : {0U, 1U, 100U}) {
    boids::BoidSimulationConfig config;
    config.boid_count = 1U;
    config.dummy_count = 1U;
    config.dummy_change_percent = percent;
    const auto input = MakeInput(1U, 1U);
    BoidSimulation simulation(config);
    simulation.Initialize(input);
    TimingRecorder disabled;
    disabled.set_enabled(false);
    if (percent == 100U) {
      EXPECT_THROW(simulation.Update(0.02F), std::logic_error);
      EXPECT_THROW(simulation.Update(0.02F, disabled), std::logic_error);
      const std::array<std::size_t, 1> ids{1U};
      EXPECT_NO_THROW(simulation.Update(0.02F, ids, disabled));
    } else {
      EXPECT_NO_THROW(simulation.Update(0.02F));
      EXPECT_NO_THROW(simulation.Update(0.02F, disabled));
    }
  }
}

TEST(OopSimulationTest, RecordsEverySectionAndExplicitZerosInSameRun) {
  boids::BoidSimulationConfig config;
  config.boid_count = 2U;
  config.dummy_count = 2U;
  config.dummy_change_percent = 50U;
  const auto input = MakeInput(2U, 2U);
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  TimingRecorder timing({"entity_changes", "query_preparation",
                         "neighbor_calculation", "state_update"});
  timing.begin_measurement(2U);
  const std::array<std::size_t, 1> ids{1U};
  for (std::size_t step = 0; step < 2; ++step) {
    timing.begin_step_measurement();
    simulation.Update(0.02F,
                      step == 0U ? std::span<const std::size_t>{}
                                 : std::span<const std::size_t>{ids},
                      timing);
    timing.end_step_measurement();
  }
  const auto result = timing.result();
  ASSERT_EQ(result.step_ms.size(), 2U);
  ASSERT_EQ(result.sections.size(), 4U);
  for (const auto& section : result.sections) {
    EXPECT_EQ(section.recorded_steps, 2U);
    EXPECT_TRUE(section.mean_ms_per_step.has_value());
  }
  EXPECT_EQ(result.sections[0].samples[0].calls, 0U);
  EXPECT_DOUBLE_EQ(result.sections[0].samples[0].elapsed_ms, 0.0);
  EXPECT_EQ(result.sections[0].samples[1].calls, 1U);
  EXPECT_EQ(result.sections[1].calls, 0U);
  EXPECT_DOUBLE_EQ(result.sections[1].total_ms, 0.0);
  EXPECT_EQ(result.sections[2].calls, 2U);
  EXPECT_EQ(result.sections[3].calls, 2U);
}

TEST(OopSimulationTest,
     CountsPointersDerivedObjectsAndIndicesWithoutBorrowedInput) {
  boids::BoidSimulationConfig config;
  config.boid_count = 2U;
  config.dummy_count = 3U;
  auto input = MakeInput(2U, 3U);
  input.dummy_steps = {{1U}, {3U}};
  BoidSimulation simulation(config);
  simulation.Initialize(input);
  const auto report = simulation.GetStorageReport();
  EXPECT_TRUE(report.complete());
  EXPECT_EQ(report.entries().size(), 5U);
  const auto expected = sizeof(BoidSimulation) +
                        5U * sizeof(std::unique_ptr<SimulationObject>) +
                        5U * sizeof(std::size_t) + 2U * sizeof(BoidObject) +
                        3U * sizeof(DummyObject);
  EXPECT_EQ(report.size_bytes(), expected);
  EXPECT_GE(report.capacity_bytes(), expected);
  for (const auto& entry : report.entries()) {
    EXPECT_FALSE(std::string_view{entry.name}.starts_with("input."));
  }
}

// Exercise the actual Common generator through the OOP simulation API.
TEST(OopCommonInputIntegrationTest, AppliesWarmupThenRemainingMeasuredPlans) {
  struct Scenario {
    std::size_t boids;
    std::size_t dummies;
    std::uint32_t percent;
  };
  const std::array<Scenario, 7> scenarios{{{0U, 0U, 0U},
                                           {0U, 4U, 100U},
                                           {3U, 0U, 0U},
                                           {3U, 4U, 0U},
                                           {3U, 4U, 1U},
                                           {3U, 4U, 50U},
                                           {3U, 4U, 100U}}};
  for (const auto& scenario : scenarios) {
    SCOPED_TRACE(::testing::Message() << "boids=" << scenario.boids
                                      << ", dummies=" << scenario.dummies
                                      << ", percent=" << scenario.percent);
    boids::BoidSimulationConfig config;
    config.boid_count = scenario.boids;
    config.dummy_count = scenario.dummies;
    config.dummy_change_percent = scenario.percent;
    const auto input = boids::PrepareSimulationInput(config, 5U);
    BoidSimulation simulation(config);
    simulation.Initialize(input);
    TimingRecorder timing({"entity_changes", "query_preparation",
                           "neighbor_calculation", "state_update"});
    timing.set_enabled(false);
    for (std::size_t step = 0; step < 5U; ++step) {
      if (step == 2U) timing.begin_measurement(3U);
      if (step >= 2U) timing.begin_step_measurement();
      simulation.Update(0.02F, input.dummy_steps[step], timing);
      if (step >= 2U) timing.end_step_measurement();
      ExpectInitialDummies(simulation, input);
      const auto& plan = input.dummy_steps[step];
      for (std::size_t index = 0; index < plan.size(); ++index) {
        EXPECT_EQ(
            simulation
                .Objects()[simulation.Objects().size() - plan.size() + index]
                ->LogicalId(),
            plan[index]);
      }
    }
    EXPECT_EQ(timing.result().measured_steps, 3U);
    EXPECT_EQ(timing.result().sections[0].calls,
              input.dummy_steps.front().empty() ? 0U : 3U);
  }
}

TEST(OopCommonInputIntegrationTest,
     OwnsConvenienceInputAndReleasesItWhenBorrowing) {
  boids::BoidSimulationConfig config;
  config.boid_count = 3U;
  BoidSimulation simulation(config);
  simulation.Initialize();
  EXPECT_NO_THROW(simulation.Update(0.02F));
  const auto owned = simulation.GetStorageReport();
  bool found_input = false;
  for (const auto& entry : owned.entries()) {
    if (entry.name == "input.initial_objects") {
      found_input = true;
      EXPECT_EQ(entry.size_bytes, 3U * sizeof(boids::InitialObjectState));
    }
  }
  EXPECT_TRUE(found_input);
  const auto input = boids::PrepareSimulationInput(config, 0U);
  simulation.Initialize(input);
  const auto borrowed = simulation.GetStorageReport();
  EXPECT_EQ(owned.size_bytes() - borrowed.size_bytes(),
            sizeof(SimulationInput) + 3U * sizeof(boids::InitialObjectState));
  EXPECT_NO_THROW(simulation.Update(0.0F));
}

}  // namespace
