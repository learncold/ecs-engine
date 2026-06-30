#pragma once

#include "Engine/ECS/Entity.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Engine::ECS {

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
    Component& Emplace(Entity entity, Args&&... args)
    {
        EnsureSparseSize(entity);

        if (Contains(entity)) {
            dense_components_[sparse_[entity.Value()]] = Component{std::forward<Args>(args)...};
            return dense_components_[sparse_[entity.Value()]];
        }

        sparse_[entity.Value()] = dense_entities_.size();
        dense_entities_.push_back(entity);
        dense_components_.push_back(Component{std::forward<Args>(args)...});
        return dense_components_.back();
    }

    void Remove(Entity entity) override
    {
        if (!Contains(entity)) {
            return;
        }

        const std::size_t removedIndex = sparse_[entity.Value()];
        const std::size_t lastIndex = dense_entities_.size() - 1U;

        if (removedIndex != lastIndex) {
            dense_components_[removedIndex] = std::move(dense_components_[lastIndex]);
            dense_entities_[removedIndex] = dense_entities_[lastIndex];
            sparse_[dense_entities_[removedIndex].Value()] = removedIndex;
        }

        dense_components_.pop_back();
        dense_entities_.pop_back();
        sparse_[entity.Value()] = MissingIndex;
    }

    void Clear() override
    {
        sparse_.clear();
        dense_entities_.clear();
        dense_components_.clear();
    }

    [[nodiscard]] bool Contains(Entity entity) const
    {
        const Entity::IdType entityId = entity.Value();
        return entity.IsValid()
            && entityId < sparse_.size()
            && sparse_[entityId] != MissingIndex
            && sparse_[entityId] < dense_entities_.size()
            && dense_entities_[sparse_[entityId]] == entity;
    }

    [[nodiscard]] Component& Get(Entity entity)
    {
        if (!Contains(entity)) {
            throw std::out_of_range("Entity does not own requested component");
        }

        return dense_components_[sparse_[entity.Value()]];
    }

    [[nodiscard]] const Component& Get(Entity entity) const
    {
        if (!Contains(entity)) {
            throw std::out_of_range("Entity does not own requested component");
        }

        return dense_components_[sparse_[entity.Value()]];
    }

    [[nodiscard]] Component* TryGet(Entity entity)
    {
        return Contains(entity) ? &dense_components_[sparse_[entity.Value()]] : nullptr;
    }

    [[nodiscard]] const Component* TryGet(Entity entity) const
    {
        return Contains(entity) ? &dense_components_[sparse_[entity.Value()]] : nullptr;
    }

    [[nodiscard]] const std::vector<Entity>& Entities() const
    {
        return dense_entities_;
    }

    [[nodiscard]] std::size_t Size() const
    {
        return dense_components_.size();
    }

private:
    static constexpr std::size_t MissingIndex = std::numeric_limits<std::size_t>::max();

    //특정 Entity에 대한 sparse 벡터 크기를 확보
    void EnsureSparseSize(Entity entity)
    {
        if (!entity.IsValid()) {
            throw std::invalid_argument("Cannot store a component for an invalid entity");
        }

        const std::size_t requiredSize = static_cast<std::size_t>(entity.Value()) + 1U;
        if (requiredSize > sparse_.size()) {
            sparse_.resize(requiredSize, MissingIndex);
        }
    }

    std::vector<std::size_t> sparse_;
    std::vector<Entity> dense_entities_;
    std::vector<Component> dense_components_;
};

} // namespace Engine::ECS
