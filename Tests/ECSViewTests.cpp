#include "Engine/ECS/Registry.h"

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
    Engine::ECS::Registry registry;

    const Engine::ECS::Entity movingEntity = registry.Create();
    const Engine::ECS::Entity transformOnlyEntity = registry.Create();
    const Engine::ECS::Entity velocityOnlyEntity = registry.Create();

    registry.Emplace<Transform>(movingEntity, 1.0F, 2.0F, 3.0F);
    registry.Emplace<Velocity>(movingEntity, 0.5F, 1.5F, 2.5F);

    registry.Emplace<Transform>(transformOnlyEntity, 10.0F, 20.0F, 30.0F);
    registry.Emplace<Velocity>(velocityOnlyEntity, 100.0F, 200.0F, 300.0F);

    int entityCallbackCount = 0;
    registry.view<Transform, Velocity>().each(
        [&](Engine::ECS::Entity entity, Transform& transform, Velocity& velocity) {
            assert(entity == movingEntity);
            transform.x += velocity.x;
            transform.y += velocity.y;
            transform.z += velocity.z;
            ++entityCallbackCount;
        });

    assert(entityCallbackCount == 1);
    assert(registry.Get<Transform>(movingEntity).x == 1.5F);
    assert(registry.Get<Transform>(movingEntity).y == 3.5F);
    assert(registry.Get<Transform>(movingEntity).z == 5.5F);

    int componentCallbackCount = 0;
    registry.view<Transform, Velocity>().each(
        [&](Transform& transform, Velocity& velocity) {
            velocity.x += transform.x;
            velocity.y += transform.y;
            velocity.z += transform.z;
            ++componentCallbackCount;
        });

    assert(componentCallbackCount == 1);
    assert(registry.Get<Velocity>(movingEntity).x == 2.0F);
    assert(registry.Get<Velocity>(movingEntity).y == 5.0F);
    assert(registry.Get<Velocity>(movingEntity).z == 8.0F);

    int missingStorageCallbackCount = 0;
    registry.view<Transform, BoidTag>().each(
        [&](Engine::ECS::Entity entity, Transform& transform, BoidTag& tag) {
            (void)entity;
            (void)transform;
            (void)tag;
            ++missingStorageCallbackCount;
        });

    assert(missingStorageCallbackCount == 0);
    return 0;
}
