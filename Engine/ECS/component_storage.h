#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Engine/ECS/entity.h"

namespace engine::ecs {

using DenseIndex = Entity::IdType;

template <typename... Components>
class View;

template <typename... Components>
class CachedView;

class IComponentStorage {
 public:
  virtual ~IComponentStorage() = default;

  virtual void Remove(Entity entity) = 0;
  virtual void Clear() = 0;
};

template <typename Component>
class ComponentStorage final : public IComponentStorage {
 public:
  static constexpr DenseIndex kInvalidDenseIndex = Entity::kInvalidId;

  ComponentStorage() = default;

  template <typename... Args>
  Component& Emplace(Entity entity, Args&&... args) {
    const DenseIndex existing_index = FindDenseIndex(entity);
    if (existing_index != kInvalidDenseIndex) {
      dense_components_[existing_index] =
          Component{std::forward<Args>(args)...};
      return dense_components_[existing_index];
    }

    EnsureSparseSize(entity);

    sparse_[entity.Value()] =
        static_cast<DenseIndex>(dense_entities_.size());
    dense_entities_.push_back(entity);
    dense_components_.push_back(Component{std::forward<Args>(args)...});
    ++structural_revision_;
    return dense_components_.back();
  }

  void Remove(Entity entity) override {
    const DenseIndex removed_index = FindDenseIndex(entity);
    if (removed_index == kInvalidDenseIndex) {
      return;
    }

    const DenseIndex last_index =
        static_cast<DenseIndex>(dense_entities_.size() - 1U);

    if (removed_index != last_index) {
      dense_components_[removed_index] =
          std::move(dense_components_[last_index]);
      dense_entities_[removed_index] = dense_entities_[last_index];
      sparse_[dense_entities_[removed_index].Value()] = removed_index;
    }

    dense_components_.pop_back();
    dense_entities_.pop_back();
    sparse_[entity.Value()] = kInvalidDenseIndex;
    ++structural_revision_;
  }

  void Clear() override {
    if (!dense_entities_.empty()) {
      ++structural_revision_;
    }
    sparse_.clear();
    dense_entities_.clear();
    dense_components_.clear();
  }

  [[nodiscard]] bool Contains(Entity entity) const {
    return FindDenseIndex(entity) != kInvalidDenseIndex;
  }

  [[nodiscard]] DenseIndex FindDenseIndex(Entity entity) const noexcept {
    const Entity::IdType entity_id = entity.Value();
    if (!entity.IsValid() || entity_id >= sparse_.size()) {
      return kInvalidDenseIndex;
    }

    const DenseIndex dense_index = sparse_[entity_id];
    if (dense_index == kInvalidDenseIndex ||
        dense_index >= dense_entities_.size() ||
        dense_entities_[dense_index] != entity) {
      return kInvalidDenseIndex;
    }

    return dense_index;
  }

  [[nodiscard]] Component* FindComponent(Entity entity) noexcept {
    const DenseIndex dense_index = FindDenseIndex(entity);
    return dense_index != kInvalidDenseIndex ? &dense_components_[dense_index]
                                             : nullptr;
  }

  [[nodiscard]] const Component* FindComponent(Entity entity) const noexcept {
    const DenseIndex dense_index = FindDenseIndex(entity);
    return dense_index != kInvalidDenseIndex ? &dense_components_[dense_index]
                                             : nullptr;
  }

  [[nodiscard]] const std::vector<Entity>& Entities() const {
    return dense_entities_;
  }

  [[nodiscard]] std::size_t Size() const { return dense_components_.size(); }

 private:
  template <typename... Components>
  friend class View;

  template <typename... Components>
  friend class CachedView;

  // ensure sparse vector size for some entity
  void EnsureSparseSize(Entity entity) {
    if (!entity.IsValid()) {
      throw std::invalid_argument(
          "Cannot store a component for an invalid entity");
    }

    const std::size_t required_size =
        static_cast<std::size_t>(entity.Value()) + 1U;
    if (required_size > sparse_.size()) {
      sparse_.resize(required_size, kInvalidDenseIndex);
    }
  }

  // Precondition: dense_index belongs to this storage and no structural
  // modification has occurred since it was resolved.
  [[nodiscard]] Component& ComponentAtDenseIndexUnchecked(
      DenseIndex dense_index) noexcept {
    return dense_components_[dense_index];
  }

  [[nodiscard]] const Component& ComponentAtDenseIndexUnchecked(
      DenseIndex dense_index) const noexcept {
    return dense_components_[dense_index];
  }

  [[nodiscard]] std::uint64_t StructuralRevision() const noexcept {
    return structural_revision_;
  }

  std::vector<DenseIndex> sparse_;
  std::vector<Entity> dense_entities_;
  std::vector<Component> dense_components_;
  std::uint64_t structural_revision_ = 0U;
};

}  // namespace engine::ecs
