#pragma once

#include <cstdint>
#include <limits>

namespace engine::ecs {

class Entity {
 public:
  using IdType = std::uint32_t;

  static constexpr IdType kInvalidId = std::numeric_limits<IdType>::max();

  constexpr Entity() = default;
  explicit constexpr Entity(IdType value) : value_(value) {}

  [[nodiscard]] constexpr IdType Value() const { return value_; }

  [[nodiscard]] constexpr bool IsValid() const { return value_ != kInvalidId; }

  friend constexpr bool operator==(Entity left, Entity right) {
    return left.value_ == right.value_;
  }

  friend constexpr bool operator!=(Entity left, Entity right) {
    return !(left == right);
  }

 private:
  IdType value_ = kInvalidId;
};

}  // namespace engine::ecs
