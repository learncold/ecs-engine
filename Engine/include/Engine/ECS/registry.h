#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine/ECS/component_storage.h"
#include "Engine/ECS/entity.h"

namespace engine::ecs {

template <typename... Components>
class View;

class Registry {
 public:
  Registry() = default;

  [[nodiscard]] Entity Create() {
    Entity entity(next_entity_id_++);

    const std::size_t required_size =
        static_cast<std::size_t>(entity.Value()) + 1U;
    if (required_size > alive_entities_.size()) {
      alive_entities_.resize(required_size, std::uint8_t{0});
    }

    alive_entities_[entity.Value()] = std::uint8_t{1};
    ++living_entity_count_;
    return entity;
  }

  void Destroy(Entity entity) {
    if (!IsAlive(entity)) {
      return;
    }

    for (auto& storage_entry : component_storages_) {
      storage_entry.second->Remove(entity);
    }

    alive_entities_[entity.Value()] = std::uint8_t{0};
    --living_entity_count_;
  }

  [[nodiscard]] bool IsAlive(Entity entity) const {
    const Entity::IdType entity_id = entity.Value();
    return entity.IsValid() && entity_id < alive_entities_.size() &&
           alive_entities_[entity_id] != std::uint8_t{0};
  }

  [[nodiscard]] std::size_t EntityCount() const { return living_entity_count_; }

  template <typename Component, typename... Args>
  Component& Emplace(Entity entity, Args&&... args) {
    ValidateAliveEntity(entity);
    return GetOrCreateStorage<Component>().Emplace(entity,
                                                   std::forward<Args>(args)...);
  }

  template <typename Component>
  void Remove(Entity entity) {
    if (auto* storage = FindStorage<Component>()) {
      storage->Remove(entity);
    }
  }

  template <typename Component>
  [[nodiscard]] bool Has(Entity entity) const {
    const auto* storage = FindStorage<Component>();
    return storage != nullptr && storage->Contains(entity);
  }

  template <typename Component>
  [[nodiscard]] Component& Get(Entity entity) {
    auto* storage = FindStorage<Component>();
    if (storage == nullptr) {
      throw std::out_of_range("Requested component storage does not exist");
    }

    return storage->Get(entity);
  }

  template <typename Component>
  [[nodiscard]] const Component& Get(Entity entity) const {
    const auto* storage = FindStorage<Component>();
    if (storage == nullptr) {
      throw std::out_of_range("Requested component storage does not exist");
    }

    return storage->Get(entity);
  }

  template <typename Component>
  [[nodiscard]] Component* TryGet(Entity entity) {
    auto* storage = FindStorage<Component>();
    return storage != nullptr ? storage->TryGet(entity) : nullptr;
  }

  template <typename Component>
  [[nodiscard]] const Component* TryGet(Entity entity) const {
    const auto* storage = FindStorage<Component>();
    return storage != nullptr ? storage->TryGet(entity) : nullptr;
  }

  template <typename... Components>
  [[nodiscard]] View<Components...> CreateView() {
    return View<Components...>(*this);
  }

  void Clear() {
    component_storages_.clear();
    alive_entities_.clear();
    living_entity_count_ = 0U;
    next_entity_id_ = 0U;
  }

 private:
  template <typename Component>
  ComponentStorage<Component>& GetOrCreateStorage() {
    const std::type_index type_key(typeid(Component));
    auto storage_iterator = component_storages_.find(type_key);

    if (storage_iterator == component_storages_.end()) {
      auto inserted = component_storages_.emplace(
          type_key, std::make_unique<ComponentStorage<Component>>());
      storage_iterator = inserted.first;
    }

    return static_cast<ComponentStorage<Component>&>(*storage_iterator->second);
  }

  template <typename Component>
  [[nodiscard]] ComponentStorage<Component>* FindStorage() {
    const std::type_index type_key(typeid(Component));
    const auto storage_iterator = component_storages_.find(type_key);

    if (storage_iterator == component_storages_.end()) {
      return nullptr;
    }

    return static_cast<ComponentStorage<Component>*>(
        storage_iterator->second.get());
  }

  template <typename Component>
  [[nodiscard]] const ComponentStorage<Component>* FindStorage() const {
    const std::type_index type_key(typeid(Component));
    const auto storage_iterator = component_storages_.find(type_key);

    if (storage_iterator == component_storages_.end()) {
      return nullptr;
    }

    return static_cast<const ComponentStorage<Component>*>(
        storage_iterator->second.get());
  }

  void ValidateAliveEntity(Entity entity) const {
    if (!IsAlive(entity)) {
      throw std::invalid_argument("Entity is not alive in this registry");
    }
  }

  Entity::IdType next_entity_id_ = 0U;
  std::size_t living_entity_count_ = 0U;
  std::vector<std::uint8_t> alive_entities_;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>>
      component_storages_;

  template <typename... Components>
  friend class View;
};

template <typename... Components>
class View {
  static_assert(sizeof...(Components) > 0U,
                "View requires at least one component type");

 public:
  explicit View(Registry& registry)
      : storages_(registry.template FindStorage<Components>()...) {}

  template <typename Function>
  void Each(Function&& function) {
    if (!HasRequiredStorages()) {
      return;
    }

    for (Entity entity : DriverStorage().Entities()) {
      if (ContainsAll(entity)) {
        Invoke(function, entity);
      }
    }
  }

 private:
  template <typename>
  static constexpr bool kAlwaysFalse = false;

  using StorageTuple = std::tuple<ComponentStorage<Components>*...>;
  using DriverComponent = std::tuple_element_t<0U, std::tuple<Components...>>;

  [[nodiscard]] bool HasRequiredStorages() const {
    return std::apply(
        [](const auto*... storages) { return ((storages != nullptr) && ...); },
        storages_);
  }

  [[nodiscard]] ComponentStorage<DriverComponent>& DriverStorage() {
    return *std::get<0U>(storages_);
  }

  [[nodiscard]] bool ContainsAll(Entity entity) const {
    return std::apply(
        [entity](const auto*... storages) {
          return (storages->Contains(entity) && ...);
        },
        storages_);
  }

  template <typename Function>
  void Invoke(Function& function, Entity entity) {
    InvokeWithComponents(function, entity,
                         std::index_sequence_for<Components...>{});
  }

  template <typename Function, std::size_t... component_indices>
  void InvokeWithComponents(Function& function, Entity entity,
                            std::index_sequence<component_indices...>) {
    if constexpr (std::is_invocable_v<Function&, Entity, Components&...>) {
      std::invoke(function, entity,
                  (*std::get<component_indices>(storages_)->TryGet(entity))...);
    } else if constexpr (std::is_invocable_v<Function&, Components&...>) {
      std::invoke(function,
                  (*std::get<component_indices>(storages_)->TryGet(entity))...);
    } else {
      static_assert(kAlwaysFalse<Function>,
                    "View callback must accept (Entity, Components&...) or "
                    "(Components&...)");
    }
  }

  StorageTuple storages_;
};

}  // namespace engine::ecs
