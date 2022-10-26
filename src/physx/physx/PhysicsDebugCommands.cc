#include "console/Console.hh"
#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"

#include <glm/glm.hpp>
#include <limits>

bool floatEqual(float a, float b) {
    float feps = std::numeric_limits<float>::epsilon() * 5.0f;
    return (a - feps < b) && (a + feps > b);
}

void assertEqual(glm::vec3 a, glm::vec3 b) {
    if (!floatEqual(a.x, b.x) || !floatEqual(a.y, b.y) || !floatEqual(a.z, b.z)) {
        Errorf("Assertion failed: %s != %s", glm::to_string(a), glm::to_string(b));
    }
}

void sp::PhysxManager::RegisterDebugCommands() {
    funcs.Register<ecs::EntityRef, glm::vec3>("assert_position",
        "Asserts an entity is located at the specified position in world-space (assert_position <entity> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, glm::vec3 expected) {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::TransformTree>>();
            auto entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Abortf("Entity does not exist: %s", entityRef.Name());
            } else if (!entity.Has<ecs::TransformTree>(lock)) {
                Abortf("Entity has no TransformTree component: %s", entityRef.Name());
            }
            auto transform = entity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
            assertEqual(transform.GetPosition(), expected);
        });

    funcs.Register<ecs::EntityRef, glm::vec3>("assert_velocity",
        "Asserts an entity's velocity is equal to the value in world-space (assert_velocity <entity> <dx> <dy> <dz>)",
        [this](ecs::EntityRef entityRef, glm::vec3 expected) {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::TransformTree>>();
            auto entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Abortf("Entity does not exist: %s", entityRef.Name());
            } else if (!entity.Has<ecs::TransformTree>(lock)) {
                Abortf("Entity has no TransformTree component: %s", entityRef.Name());
            }
            auto velocity = GetEntityVelocity(lock, entity);
            assertEqual(velocity, expected);
        });
}
