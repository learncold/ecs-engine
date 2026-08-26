#include "Engine/ECS/registry.h"

#include <cassert>

namespace {

struct Transform {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Velocity {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct BoidTag {
};

} // namespace

int main()
{
    engine::ecs::Registry registry;

    const engine::ecs::Entity moving_entity = registry.Create();
    const engine::ecs::Entity transform_only_entity = registry.Create();
    const engine::ecs::Entity velocity_only_entity = registry.Create();

    registry.Emplace<Transform>(moving_entity, 1.0F, 2.0F, 3.0F);
    registry.Emplace<Velocity>(moving_entity, 0.5F, 1.5F, 2.5F);

    registry.Emplace<Transform>(transform_only_entity, 10.0F, 20.0F, 30.0F);
    registry.Emplace<Velocity>(velocity_only_entity, 100.0F, 200.0F, 300.0F);

    int entity_callback_count = 0;
    registry.CreateView<Transform, Velocity>().Each(
        [&](engine::ecs::Entity entity, Transform& transform, Velocity& velocity) {
            assert(entity == moving_entity);
            transform.x += velocity.x;
            transform.y += velocity.y;
            transform.z += velocity.z;
            ++entity_callback_count;
        });

    assert(entity_callback_count == 1);
    assert(registry.Get<Transform>(moving_entity).x == 1.5F);
    assert(registry.Get<Transform>(moving_entity).y == 3.5F);
    assert(registry.Get<Transform>(moving_entity).z == 5.5F);

    int component_callback_count = 0;
    registry.CreateView<Transform, Velocity>().Each(
        [&](Transform& transform, Velocity& velocity) {
            velocity.x += transform.x;
            velocity.y += transform.y;
            velocity.z += transform.z;
            ++component_callback_count;
        });

    assert(component_callback_count == 1);
    assert(registry.Get<Velocity>(moving_entity).x == 2.0F);
    assert(registry.Get<Velocity>(moving_entity).y == 5.0F);
    assert(registry.Get<Velocity>(moving_entity).z == 8.0F);

    int missing_storage_callback_count = 0;
    registry.CreateView<Transform, BoidTag>().Each(
        [&](engine::ecs::Entity entity, Transform& transform, BoidTag& tag) {
            (void)entity;
            (void)transform;
            (void)tag;
            ++missing_storage_callback_count;
        });

    assert(missing_storage_callback_count == 0);
    return 0;
}
