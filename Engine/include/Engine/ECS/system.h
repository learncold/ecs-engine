#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "Engine/ECS/registry.h"

namespace engine::ecs {

class System {
 public:
  virtual ~System() = default;

  virtual void Update(Registry& registry, float delta_seconds) = 0;
};

class SystemManager {
 public:
  template <typename SystemType, typename... Args>
  SystemType& Add(Args&&... args) {
    static_assert(std::is_base_of_v<System, SystemType>,
                  "SystemType must derive from engine::ecs::System");

    auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
    SystemType& system_reference = *system;
    systems_.push_back(std::move(system));
    return system_reference;
  }

  void Update(Registry& registry, float delta_seconds) {
    for (const auto& system : systems_) {
      system->Update(registry, delta_seconds);
    }
  }

  void Clear() { systems_.clear(); }

  [[nodiscard]] std::size_t Size() const { return systems_.size(); }

 private:
  std::vector<std::unique_ptr<System>> systems_;
};

}  // namespace engine::ecs
