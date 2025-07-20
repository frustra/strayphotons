/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <limits>

bool floatEqual(float a, float b) {
    float feps = std::numeric_limits<float>::epsilon() * 5.0f;
    return (a - feps < b) && (a + feps > b);
}

void assertEqual(glm::vec3 a, glm::vec3 b) {
    if (!floatEqual(a.x, b.x) || !floatEqual(a.y, b.y) || !floatEqual(a.z, b.z)) {
        Abortf("Assertion failed: %s != %s", glm::to_string(a), glm::to_string(b));
    }
}

void sp::PhysxManager::RegisterDebugCommands() {
    funcs.Register<ecs::EntityRef, glm::vec3>("set_position",
        "Sets an entity's position to the specified coordinates (set_position <entity> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, glm::vec3 position) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::TransformTree, ecs::TransformSnapshot>>();
            auto entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Abortf("Entity does not exist: %s", entityRef.Name().String());
            } else if (!entity.Has<ecs::TransformTree, ecs::TransformSnapshot>(lock)) {
                Abortf("Entity has no TransformTree and/or TransformSnapshot component: %s", entityRef.Name().String());
            }
            auto &tree = entity.Get<ecs::TransformTree>(lock);
            tree.pose.SetPosition(position);
            entity.Set<ecs::TransformSnapshot>(lock, tree.GetGlobalTransform(lock));
        });

    funcs.Register<ecs::EntityRef, glm::vec3>("set_velocity",
        "Sets an entity's velocity to the specified value in world-space (set_velocity <entity> <dx> <dy> <dz>)",
        [this](ecs::EntityRef entityRef, glm::vec3 velocity) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::Physics>>();
            auto entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Abortf("Entity does not exist: %s", entityRef.Name().String());
            } else if (actors.count(entity) == 0) {
                Abortf("Entity has no Physics actor: %s", entityRef.Name().String());
            }

            auto &actor = actors[entity];
            auto dynamic = actor->is<physx::PxRigidDynamic>();
            Assertf(dynamic != nullptr, "Entity is not a RigidDynamic actor: %s", entityRef.Name().String());
            auto userData = (ActorUserData *)actor->userData;
            Assertf(userData != nullptr, "Entity has no Physics actor user data: %s", entityRef.Name().String());
            userData->velocity = velocity;
            dynamic->setLinearVelocity(GlmVec3ToPxVec3(velocity));
        });

    funcs.Register<ecs::EntityRef, glm::vec3>("assert_position",
        "Asserts an entity is located at the specified position in world-space (assert_position <entity> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, glm::vec3 expected) {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::TransformTree>>();
            auto entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Abortf("Entity does not exist: %s", entityRef.Name().String());
            } else if (!entity.Has<ecs::TransformTree>(lock)) {
                Abortf("Entity has no TransformTree component: %s", entityRef.Name().String());
            }
            auto transform = entity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
            assertEqual(transform.GetPosition(), expected);
        });

    funcs.Register<ecs::EntityRef, glm::vec3>("assert_scale",
        "Asserts an entity's local scale matches a specified value (assert_scale <entity> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, glm::vec3 expected) {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::TransformTree>>();
            auto entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Abortf("Entity does not exist: %s", entityRef.Name().String());
            } else if (!entity.Has<ecs::TransformTree>(lock)) {
                Abortf("Entity has no TransformTree component: %s", entityRef.Name().String());
            }
            auto scale = entity.Get<ecs::TransformTree>(lock).pose.GetScale();
            assertEqual(scale, expected);
        });

    funcs.Register<ecs::EntityRef, glm::vec3>("assert_velocity",
        "Asserts an entity's velocity is equal to the value in world-space (assert_velocity <entity> <dx> <dy> <dz>)",
        [this](ecs::EntityRef entityRef, glm::vec3 expected) {
            auto lock = ecs::StartTransaction<>();
            auto entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Abortf("Entity does not exist: %s", entityRef.Name().String());
            } else if (!entity.Has<ecs::Physics>(lock)) {
                Abortf("Entity has no Physics component: %s", entityRef.Name().String());
            }

            ActorUserData *actorData = nullptr;
            if (actors.count(entity) > 0) {
                actorData = (ActorUserData *)actors[entity]->userData;
            } else if (subActors.count(entity) > 0) {
                actorData = (ActorUserData *)subActors[entity]->userData;
            } else if (controllers.count(entity) > 0) {
                auto controllerData = (CharacterControllerUserData *)controllers[entity]->getUserData();
                if (controllerData) actorData = &controllerData->actorData;
            } else {
                Assertf(expected == glm::vec3(0), "Entity has no Physics actor: %s", entityRef.Name().String());
                return;
            }
            Assertf(actorData != nullptr, "Entity has no Physics actor user data: %s", entityRef.Name().String());

            assertEqual(actorData->velocity, expected);
        });
}
