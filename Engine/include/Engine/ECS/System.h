#pragma once

#include "Engine/ECS/Registry.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace Engine::ECS {

class System {
public:
    virtual ~System() = default;

    virtual void Update(Registry& registry, float deltaSeconds) = 0;
};

class SystemManager {
public:
    template <typename SystemType, typename... Args>
    SystemType& Add(Args&&... args)
    {
        static_assert(std::is_base_of_v<System, SystemType>, "SystemType must derive from Engine::ECS::System");

        auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
        SystemType& systemReference = *system;
        systems_.push_back(std::move(system));
        return systemReference;
    }

    void Update(Registry& registry, float deltaSeconds)
    {
        for (const auto& system : systems_) {
            system->Update(registry, deltaSeconds);
        }
    }

    void Clear()
    {
        systems_.clear();
    }

    [[nodiscard]] std::size_t Size() const
    {
        return systems_.size();
    }

private:
    std::vector<std::unique_ptr<System>> systems_;
};

} // namespace Engine::ECS
