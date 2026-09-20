#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Engine/ECS/entity.h"

namespace engine::ecs {

template <typename... Components>
class View;

class IComponentStorage {
 public:
  virtual ~IComponentStorage() = default;

  virtual void Remove(Entity entity) = 0;
  virtual void Clear() = 0;
};

template <typename Component>
class ComponentStorage final : public IComponentStorage {
 public:
  ComponentStorage() = default;

  template <typename... Args>
  Component& Emplace(Entity entity, Args&&... args) {
    if (Contains(entity)) {
      dense_components_[sparse_[entity.Value()]] =
          Component{std::forward<Args>(args)...};
      return dense_components_[sparse_[entity.Value()]];
    }

    EnsureSparseSize(entity);

    sparse_[entity.Value()] = dense_entities_.size();
    dense_entities_.push_back(entity);
    dense_components_.push_back(Component{std::forward<Args>(args)...});
    return dense_components_.back();
  }

  void Remove(Entity entity) override {
    if (!Contains(entity)) {
      return;
    }

    const std::size_t removed_index = sparse_[entity.Value()];
    const std::size_t last_index = dense_entities_.size() - 1U;

    if (removed_index != last_index) {
      dense_components_[removed_index] =
          std::move(dense_components_[last_index]);
      dense_entities_[removed_index] = dense_entities_[last_index];
      sparse_[dense_entities_[removed_index].Value()] = removed_index;
    }

    dense_components_.pop_back();
    dense_entities_.pop_back();
    sparse_[entity.Value()] = Entity::kInvalidId;
  }

  void Clear() override {
    sparse_.clear();
    dense_entities_.clear();
    dense_components_.clear();
  }

  [[nodiscard]] bool Contains(Entity entity) const {
    const Entity::IdType entity_id = entity.Value();
    return entity.IsValid() && entity_id < sparse_.size() &&
           sparse_[entity_id] != Entity::kInvalidId &&
           sparse_[entity_id] < dense_entities_.size() &&
           dense_entities_[sparse_[entity_id]] == entity;
  }

  [[nodiscard]] Component* TryGetComponent(Entity entity) {
    return Contains(entity) ? &dense_components_[sparse_[entity.Value()]]
                            : nullptr;
  }

  [[nodiscard]] const Component* TryGetComponent(Entity entity) const {
    return Contains(entity) ? &dense_components_[sparse_[entity.Value()]]
                            : nullptr;
  }

  [[nodiscard]] const std::vector<Entity>& Entities() const {
    return dense_entities_;
  }

  [[nodiscard]] std::size_t Size() const { return dense_components_.size(); }

 private:
  template <typename... Components>
  friend class View;

  // Precondition: entity exists in this storage, and the storage is not
  // structurally modified between the membership check and this access.
  [[nodiscard]] Component& GetComponentUnchecked(Entity entity) noexcept {
    return dense_components_[sparse_[entity.Value()]];
  }

  [[nodiscard]] const Component& GetComponentUnchecked(
      Entity entity) const noexcept {
    return dense_components_[sparse_[entity.Value()]];
  }

  // ensure sparse vector size for some entity
  void EnsureSparseSize(Entity entity) {
    if (!entity.IsValid()) {
      throw std::invalid_argument(
          "Cannot store a component for an invalid entity");
    }

    const std::size_t required_size =
        static_cast<std::size_t>(entity.Value()) + 1U;
    if (required_size > sparse_.size()) {
      sparse_.resize(required_size, Entity::kInvalidId);
    }
  }

  std::vector<std::size_t> sparse_;
  std::vector<Entity> dense_entities_;
  std::vector<Component> dense_components_;
};

}  // namespace engine::ecs
