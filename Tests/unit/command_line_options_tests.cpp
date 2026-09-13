#include <gtest/gtest.h>

#include <array>
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
  const std::array args{"--implementation"sv, "oop"sv, "--agents"sv,
                        "500"sv, "--seed"sv, "0"sv, "--warmup"sv,
                        "0"sv, "--steps"sv, "25"sv, "--dt"sv,
                        "0.02"sv, "--benchmark"sv};
  const auto options = app::ParseCommandLineOptions(args);
  EXPECT_TRUE(options.benchmark_enabled);
  EXPECT_EQ(options.implementation, app::SimulationImplementation::Oop);
  EXPECT_EQ(options.simulation_config.boid_count, 500U);
  EXPECT_EQ(options.simulation_config.random_seed, 0U);
  EXPECT_EQ(options.benchmark_config.warmup_steps, 0U);
  EXPECT_EQ(options.benchmark_config.measurement_steps, 25U);
  EXPECT_FLOAT_EQ(options.benchmark_config.fixed_delta_seconds, 0.02F);
}

TEST(CommandLineOptionsTest, RejectsMalformedOrAmbiguousInvocations) {
  const std::vector<std::vector<std::string_view>> cases{
      {"--unknown"},
      {"--benchmark", "--steps"},
      {"--steps", "3"},
      {"--benchmark", "--steps", "0"},
      {"--benchmark", "--agents", "0"},
      {"--benchmark", "--agents", "-1"},
      {"--benchmark", "--implementation", "invalid"},
      {"--benchmark", "--steps", "3junk"},
      {"--benchmark", "--seed", "4294967296"},
      {"--benchmark", "--dt", "nan"},
      {"--benchmark", "--dt", "inf"},
      {"--benchmark", "--dt", "0"}};
  for (const auto& args : cases) {
    EXPECT_THROW(static_cast<void>(app::ParseCommandLineOptions(args)),
                 std::invalid_argument);
  }
}

}  // namespace
