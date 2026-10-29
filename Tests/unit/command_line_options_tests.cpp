#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "App/command_line_options.h"

namespace {
using namespace std::string_view_literals;

TEST(CommandLineOptionsTest, DefaultsToInteractiveAndSupportsHelp) {
  EXPECT_FALSE(app::ParseCommandLineOptions({}).benchmark_enabled);
  const std::array args{"--help"sv};
  EXPECT_TRUE(app::ParseCommandLineOptions(args).help);
}

TEST(CommandLineOptionsTest, ParsesExplicitExperimentConditions) {
  const std::array args{
      "--implementation"sv, "oop"sv,         "--agents"sv,   "500"sv,
      "--seed"sv,           "0"sv,           "--warmup"sv,   "0"sv,
      "--steps"sv,          "25"sv,          "--dt"sv,       "0.02"sv,
      "--output"sv,         "results.csv"sv, "--benchmark"sv};
  const auto options = app::ParseCommandLineOptions(args);
  EXPECT_TRUE(options.benchmark_enabled);
  EXPECT_EQ(options.implementation, app::SimulationImplementation::Oop);
  EXPECT_EQ(options.simulation_config.boid_count, 500U);
  EXPECT_EQ(options.simulation_config.random_seed, 0U);
  EXPECT_EQ(options.benchmark_config.warmup_steps, 0U);
  EXPECT_EQ(options.benchmark_config.measurement_steps, 25U);
  EXPECT_FLOAT_EQ(options.benchmark_config.fixed_delta_seconds, 0.02F);
  EXPECT_EQ(options.output_path, "results.csv");
}

TEST(CommandLineOptionsTest, AcceptsThreeImplementationsAndZeroCounts) {
  for (auto implementation : {"oop"sv, "ecs"sv, "archetype"sv}) {
    for (auto percent : {"0"sv, "40"sv, "100"sv}) {
      const std::array args{"--benchmark"sv, "--implementation"sv,
                            implementation,  "--agents"sv,
                            "0"sv,           "--dummies"sv,
                            "0"sv,           "--dummy-change-percent"sv,
                            percent};
      const auto options = app::ParseCommandLineOptions(args);
      EXPECT_EQ(options.simulation_config.boid_count, 0U);
      EXPECT_EQ(options.simulation_config.dummy_count, 0U);
      EXPECT_EQ(options.implementation,
                implementation == "oop" ? app::SimulationImplementation::Oop
                : implementation == "ecs"
                    ? app::SimulationImplementation::Ecs
                    : app::SimulationImplementation::Archetype);
    }
  }
  const std::array args{"--benchmark"sv, "--dummies"sv, "17"sv,
                        "--dummy-change-percent"sv, "40"sv};
  const auto options = app::ParseCommandLineOptions(args);
  EXPECT_EQ(options.simulation_config.dummy_count, 17U);
  EXPECT_EQ(options.simulation_config.dummy_change_percent, 40U);
  std::ostringstream usage;
  app::PrintUsage(usage);
  EXPECT_NE(usage.str().find("oop|ecs|archetype"), std::string::npos);
  EXPECT_NE(usage.str().find("--dummy-change-percent"), std::string::npos);
}

TEST(CommandLineOptionsTest, RejectsOverflowingCombinedCountsAndSteps) {
  const auto maximum = std::to_string(std::numeric_limits<std::size_t>::max());
  for (const auto& args : std::vector<std::vector<std::string_view>>{
           {"--benchmark", "--agents", maximum, "--dummies", "1"},
           {"--benchmark", "--warmup", maximum, "--steps", "1"}}) {
    EXPECT_THROW(static_cast<void>(app::ParseCommandLineOptions(args)),
                 std::invalid_argument);
  }
}

TEST(CommandLineOptionsTest, RejectsMalformedOrAmbiguousInvocations) {
  const std::vector<std::vector<std::string_view>> cases{
      {"--unknown"},
      {"--benchmark", "--steps"},
      {"--steps", "3"},
      {"--benchmark", "--steps", "0"},
      {"--benchmark", "--dummies", "-1"},
      {"--benchmark", "--dummies", "1.5"},
      {"--benchmark", "--dummies"},
      {"--benchmark", "--dummy-change-percent", "-1"},
      {"--benchmark", "--dummy-change-percent", "101"},
      {"--benchmark", "--dummy-change-percent", "4294967296"},
      {"--benchmark", "--dummy-change-percent", "nan"},
      {"--dummy-change-percent", "40"},
      {"--benchmark", "--agents", "-1"},
      {"--benchmark", "--implementation", "invalid"},
      {"--benchmark", "--steps", "3junk"},
      {"--benchmark", "--seed", "4294967296"},
      {"--benchmark", "--dt", "nan"},
      {"--benchmark", "--dt", "inf"},
      {"--benchmark", "--dt", "0"},
      {"--benchmark", "--output", ""},
      {"--output", "results.csv"}};
  for (const auto& args : cases) {
    EXPECT_THROW(static_cast<void>(app::ParseCommandLineOptions(args)),
                 std::invalid_argument);
  }
}

}  // namespace
