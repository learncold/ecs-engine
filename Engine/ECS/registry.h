#pragma once

#include <array>
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

template <typename... Components>
class CachedView;

class Registry {
 public:
  Registry() = default;

  [[nodiscard]] Entity Create() {
    Entity::IdType entity_id;

    if (!free_entity_ids_.empty()) {
      entity_id = free_entity_ids_.back();
      free_entity_ids_.pop_back();
    } else {
      entity_id = next_entity_id_++;
      const std::size_t required_size =
          static_cast<std::size_t>(entity_id) + 1U;
      alive_entities_.resize(required_size, std::uint8_t{0});
      // Keep generation history for IDs not yet recreated after Clear().
      if (generations_.size() < required_size) {
        generations_.resize(required_size, 0U);
      }
    }

    alive_entities_[entity_id] = std::uint8_t{1};
    ++living_entity_count_;

    return Entity(entity_id, generations_[entity_id]);
  }

  void Destroy(Entity entity) {
    if (!IsAlive(entity)) {
      return;
    }

    for (auto& storage_entry : component_storages_) {
      storage_entry.second->Remove(entity);
    }

    const Entity::IdType entity_id = entity.Value();

    alive_entities_[entity_id] = std::uint8_t{0};
    --living_entity_count_;
    ++generations_[entity_id];
    free_entity_ids_.push_back(entity_id);
  }

  [[nodiscard]] bool IsAlive(Entity entity) const {
    const Entity::IdType entity_id = entity.Value();
    return entity.IsValid() && entity_id < alive_entities_.size() &&
           alive_entities_[entity_id] != std::uint8_t{0} &&
           entity.Generation() == generations_[entity_id];
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
  [[nodiscard]] Component* FindComponent(Entity entity) {
    auto* storage = FindStorage<Component>();
    return storage != nullptr ? storage->FindComponent(entity) : nullptr;
  }

  template <typename Component>
  [[nodiscard]] const Component* FindComponent(Entity entity) const {
    const auto* storage = FindStorage<Component>();
    return storage != nullptr ? storage->FindComponent(entity) : nullptr;
  }

  template <typename... Components>
  [[nodiscard]] View<Components...> CreateView() {
    return View<Components...>(*this);
  }

  template <typename... Components>
  [[nodiscard]] CachedView<Components...> CreateCachedView() {
    return CachedView<Components...>(*this);
  }

  void Clear() {
    ++storage_epoch_;
    component_storages_.clear();
    alive_entities_.clear();
    living_entity_count_ = 0U;
    next_entity_id_ = 0U;
    for (auto& generation : generations_) {
      ++generation;
    }
    free_entity_ids_.clear();
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
      ++storage_epoch_;
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
  std::vector<Entity::GenerationType> generations_;
  std::vector<Entity::IdType> free_entity_ids_;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>>
      component_storages_;
  std::uint64_t storage_epoch_ = 0U;

  template <typename... Components>
  friend class View;

  template <typename... Components>
  friend class CachedView;
};

template <typename... Components>
class View {
  static_assert(sizeof...(Components) > 0U,
                "View requires at least one component type");

  static constexpr std::size_t kComponentCount = sizeof...(Components);

  using DenseIndices = std::array<DenseIndex, kComponentCount>;

 public:
  explicit View(Registry& registry)
      : storages_(registry.template FindStorage<Components>()...) {}

  template <typename Function>
  void Each(Function&& function) {
    if (!HasRequiredStorages()) {
      return;
    }

    auto& driver_storage = DriverStorage();
    const auto& driver_entities = driver_storage.Entities();

    for (DenseIndex driver_index = 0U; driver_index < driver_entities.size();
         ++driver_index) {
      const Entity entity = driver_entities[driver_index];

      DenseIndices dense_indices{};
      dense_indices[0U] = driver_index;

      if (ContainsAll(entity, dense_indices)) {
        Invoke(function, entity, dense_indices);
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

  [[nodiscard]] bool ContainsAll(Entity entity,
                                 DenseIndices& dense_indices) const {
    return ContainsAllAfterDriver(
        entity, dense_indices,
        std::make_index_sequence<sizeof...(Components) - 1U>{});
  }

  template <std::size_t... indices>
  [[nodiscard]] bool ContainsAllAfterDriver(
      Entity entity, DenseIndices& dense_indices,
      std::index_sequence<indices...>) const {
    return (((dense_indices[indices + 1U] =
                  std::get<indices + 1U>(storages_)->FindDenseIndex(entity)) !=
             std::get<indices + 1U>(storages_)->kInvalidDenseIndex) &&
            ...);
  }

  template <typename Function>
  void Invoke(Function& function, Entity entity,
              const DenseIndices& dense_indices) {
    InvokeWithComponents(function, entity, dense_indices,
                         std::index_sequence_for<Components...>{});
  }

  template <typename Function, std::size_t... component_indices>
  void InvokeWithComponents(Function& function, Entity entity,
                            const DenseIndices& dense_indices,
                            std::index_sequence<component_indices...>) {
    if constexpr (std::is_invocable_v<Function&, Entity, Components&...>) {
      std::invoke(function, entity,
                  std::get<component_indices>(storages_)
                      ->ComponentAtDenseIndexUnchecked(
                          dense_indices[component_indices])...);
    } else if constexpr (std::is_invocable_v<Function&, Components&...>) {
      std::invoke(function, std::get<component_indices>(storages_)
                                ->ComponentAtDenseIndexUnchecked(
                                    dense_indices[component_indices])...);
    } else {
      static_assert(kAlwaysFalse<Function>,
                    "View callback must accept (Entity, Components&...) or "
                    "(Components&...)");
    }
  }

  StorageTuple storages_;
};

// Snapshot view for queries that are iterated repeatedly. Component value
// changes remain visible, but structural changes invalidate the cached dense
// indices until Refresh() is called.
template <typename... Components>
class CachedView {
  static_assert(sizeof...(Components) > 0U,
                "CachedView requires at least one component type");

  static constexpr std::size_t kComponentCount = sizeof...(Components);

  using DenseIndices = std::array<DenseIndex, kComponentCount>;
  using StructuralRevisions = std::array<std::uint64_t, kComponentCount>;

  struct Entry {
    Entity entity;
    DenseIndices dense_indices;
  };

 public:
  explicit CachedView(Registry& registry) { Refresh(registry); }

  void Refresh(Registry& registry) {
    registry_ = &registry;
    storages_ = StorageTuple{registry.template FindStorage<Components>()...};
    entries_.clear();
    storage_epoch_ = registry.storage_epoch_;
    structural_revisions_.fill(0U);

    if (!HasRequiredStorages()) {
      return;
    }

    auto& driver_storage = DriverStorage();
    const auto& driver_entities = driver_storage.Entities();
    entries_.reserve(driver_entities.size());

    for (DenseIndex driver_index = 0U; driver_index < driver_entities.size();
         ++driver_index) {
      const Entity entity = driver_entities[driver_index];
      DenseIndices dense_indices{};
      dense_indices[0U] = driver_index;

      if (ContainsAll(entity, dense_indices)) {
        entries_.push_back(Entry{entity, dense_indices});
      }
    }

    CaptureStructuralRevisions(std::index_sequence_for<Components...>{});
  }

  [[nodiscard]] bool IsCurrent(const Registry& registry) const noexcept {
    if (registry_ != &registry || storage_epoch_ != registry.storage_epoch_) {
      return false;
    }

    if (!HasRequiredStorages()) {
      return true;
    }

    return StructuralRevisionsMatch(std::index_sequence_for<Components...>{});
  }

  [[nodiscard]] std::size_t Size() const noexcept { return entries_.size(); }

  template <typename Function>
  void Each(Function&& function) {
    if (registry_ == nullptr || !IsCurrent(*registry_)) {
      throw std::logic_error(
          "CachedView was invalidated by a structural registry change");
    }

    for (const Entry& entry : entries_) {
      Invoke(function, entry.entity, entry.dense_indices);
    }
  }

 private:
  template <typename>
  static constexpr bool kAlwaysFalse = false;

  using StorageTuple = std::tuple<ComponentStorage<Components>*...>;
  using DriverComponent = std::tuple_element_t<0U, std::tuple<Components...>>;

  [[nodiscard]] bool HasRequiredStorages() const noexcept {
    return std::apply(
        [](const auto*... storages) { return ((storages != nullptr) && ...); },
        storages_);
  }

  [[nodiscard]] ComponentStorage<DriverComponent>& DriverStorage() {
    return *std::get<0U>(storages_);
  }

  [[nodiscard]] bool ContainsAll(Entity entity,
                                 DenseIndices& dense_indices) const {
    return ContainsAllAfterDriver(
        entity, dense_indices,
        std::make_index_sequence<sizeof...(Components) - 1U>{});
  }

  template <std::size_t... indices>
  [[nodiscard]] bool ContainsAllAfterDriver(
      Entity entity, DenseIndices& dense_indices,
      std::index_sequence<indices...>) const {
    return (((dense_indices[indices + 1U] =
                  std::get<indices + 1U>(storages_)->FindDenseIndex(entity)) !=
             std::get<indices + 1U>(storages_)->kInvalidDenseIndex) &&
            ...);
  }

  template <std::size_t... indices>
  void CaptureStructuralRevisions(std::index_sequence<indices...>) noexcept {
    ((structural_revisions_[indices] =
          std::get<indices>(storages_)->StructuralRevision()),
     ...);
  }

  template <std::size_t... indices>
  [[nodiscard]] bool StructuralRevisionsMatch(
      std::index_sequence<indices...>) const noexcept {
    return ((structural_revisions_[indices] ==
             std::get<indices>(storages_)->StructuralRevision()) &&
            ...);
  }

  template <typename Function>
  void Invoke(Function& function, Entity entity,
              const DenseIndices& dense_indices) {
    InvokeWithComponents(function, entity, dense_indices,
                         std::index_sequence_for<Components...>{});
  }

  template <typename Function, std::size_t... component_indices>
  void InvokeWithComponents(Function& function, Entity entity,
                            const DenseIndices& dense_indices,
                            std::index_sequence<component_indices...>) {
    if constexpr (std::is_invocable_v<Function&, Entity, Components&...>) {
      std::invoke(function, entity,
                  std::get<component_indices>(storages_)
                      ->ComponentAtDenseIndexUnchecked(
                          dense_indices[component_indices])...);
    } else if constexpr (std::is_invocable_v<Function&, Components&...>) {
      std::invoke(function, std::get<component_indices>(storages_)
                                ->ComponentAtDenseIndexUnchecked(
                                    dense_indices[component_indices])...);
    } else {
      static_assert(kAlwaysFalse<Function>,
                    "CachedView callback must accept (Entity, Components&...) "
                    "or (Components&...)");
    }
  }

  Registry* registry_ = nullptr;
  StorageTuple storages_{};
  std::vector<Entry> entries_;
  StructuralRevisions structural_revisions_{};
  std::uint64_t storage_epoch_ = 0U;
};

}  // namespace engine::ecs
