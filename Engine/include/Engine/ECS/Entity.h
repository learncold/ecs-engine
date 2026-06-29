#pragma once

#include <cstdint>
#include <limits>

namespace Engine::ECS {

class Entity {
public:
    using IdType = std::uint32_t;

    static constexpr IdType InvalidId = std::numeric_limits<IdType>::max();

    constexpr Entity() = default;
    explicit constexpr Entity(IdType value)
        : value_(value)
    {
    }

    [[nodiscard]] constexpr IdType Value() const
    {
        return value_;
    }

    [[nodiscard]] constexpr bool IsValid() const
    {
        return value_ != InvalidId;
    }

    friend constexpr bool operator==(Entity left, Entity right)
    {
        return left.value_ == right.value_;
    }

    friend constexpr bool operator!=(Entity left, Entity right)
    {
        return !(left == right);
    }

private:
    IdType value_ = InvalidId;
};

} // namespace Engine::ECS
