#include <gtest/gtest.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

#include "Engine/Renderer/transform.h"

namespace {

constexpr float kTolerance = 0.00001F;

void ExpectVec3Near(const glm::vec3& actual, const glm::vec3& expected) {
  EXPECT_NEAR(actual.x, expected.x, kTolerance);
  EXPECT_NEAR(actual.y, expected.y, kTolerance);
  EXPECT_NEAR(actual.z, expected.z, kTolerance);
}

TEST(TransformTest, DefaultTransformProducesIdentityMatrix) {
  const glm::mat4 actual =
      engine::renderer::ToModelMatrix(engine::renderer::Transform{});
  const glm::mat4 identity{1.0F};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      EXPECT_NEAR(actual[column][row], identity[column][row], kTolerance);
    }
  }
}

TEST(TransformTest, AppliesTranslationRotationAndScaleInTrsOrder) {
  const engine::renderer::Transform transform{
      .position = {3.0F, 4.0F, 5.0F},
      .rotation = glm::angleAxis(glm::half_pi<float>(),
                                 glm::vec3{0.0F, 1.0F, 0.0F}),
      .scale = 2.0F};

  const glm::vec3 transformed = glm::vec3{
      engine::renderer::ToModelMatrix(transform) *
      glm::vec4{0.0F, 0.0F, 1.0F, 1.0F}};

  ExpectVec3Near(transformed, {5.0F, 4.0F, 5.0F});
}

TEST(TransformTest, DirectionRotationAlignsLocalForward) {
  const glm::vec3 direction = glm::normalize(glm::vec3{1.0F, 2.0F, -3.0F});
  const glm::quat rotation = engine::renderer::RotationFromDirection(direction);
  ExpectVec3Near(rotation * glm::vec3{0.0F, 0.0F, 1.0F}, direction);
}

TEST(TransformTest, DirectionRotationHandlesVerticalDirection) {
  const glm::quat rotation =
      engine::renderer::RotationFromDirection({0.0F, 1.0F, 0.0F});
  ExpectVec3Near(rotation * glm::vec3{0.0F, 0.0F, 1.0F},
                 {0.0F, 1.0F, 0.0F});
  EXPECT_TRUE(std::isfinite(rotation.w));
  EXPECT_TRUE(std::isfinite(rotation.x));
  EXPECT_TRUE(std::isfinite(rotation.y));
  EXPECT_TRUE(std::isfinite(rotation.z));
}

TEST(TransformTest, ZeroDirectionProducesIdentityRotation) {
  const glm::quat rotation =
      engine::renderer::RotationFromDirection(glm::vec3{0.0F});
  EXPECT_NEAR(rotation.w, 1.0F, kTolerance);
  EXPECT_NEAR(rotation.x, 0.0F, kTolerance);
  EXPECT_NEAR(rotation.y, 0.0F, kTolerance);
  EXPECT_NEAR(rotation.z, 0.0F, kTolerance);
}

}  // namespace
