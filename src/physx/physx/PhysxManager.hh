/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "console/CFunc.hh"
#include "cooking/ConvexHull.hh"
#include "common/Common.hh"
#include "common/DispatchQueue.hh"
#include "common/EntityMap.hh"
#include "common/LockFreeEventQueue.hh"
#include "common/Logging.hh"
#include "common/PreservingMap.hh"
#include "common/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/PhysicsJoints.hh"
#include "ecs/components/Transform.h"
#include "physx/AnimationSystem.hh"
#include "physx/CharacterControlSystem.hh"
#include "physx/ConstraintSystem.hh"
#include "physx/LaserSystem.hh"
#include "physx/PhysicsQuerySystem.hh"
#include "physx/SimulationCallbackHandler.hh"
#include "physx/TriggerSystem.hh"

#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultAllocator.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ecs {
    struct PhysxActorDesc;
}

namespace sp {
    class Gltf;
    struct HullSettings;
    class SceneManager;
    class ForceConstraint;
    class NoClipConstraint;

    struct ShapeUserData {
        ecs::Entity owner; // SubActor physics source entity
        size_t ownerShapeIndex;
        ecs::Entity parent; // Physics actor shape is attached to

        ecs::PhysicsShape shapeCache;
        ecs::Transform shapeTransform;
        std::shared_ptr<const ConvexHullSet> hullCache;
        std::shared_ptr<physx::PxMaterial> material;

        ShapeUserData(ecs::Entity owner,
            size_t ownerShapeIndex,
            ecs::Entity parent,
            const std::shared_ptr<physx::PxMaterial> &material)
            : owner(owner), ownerShapeIndex(ownerShapeIndex), parent(parent), material(material) {}
    };

    struct ActorUserData {
        ecs::Entity entity;
        ecs::Transform pose;
        glm::vec3 scale = glm::vec3(1);
        glm::vec3 velocity = glm::vec3(0);
        glm::vec3 gravity = glm::vec3(0);
        float angularDamping = 0.0f;
        float linearDamping = 0.0f;
        float contactReportThreshold = -1.0f;
        ecs::PhysicsGroup physicsGroup = ecs::PhysicsGroup::NoClip;

        ActorUserData() {}
        ActorUserData(ecs::Entity ent, ecs::PhysicsGroup group) : entity(ent), physicsGroup(group) {}
        ActorUserData(ecs::Entity ent, const ecs::Transform &pose, ecs::PhysicsGroup group)
            : entity(ent), pose(pose), scale(pose.GetScale()), physicsGroup(group) {}
    };

    struct CharacterControllerUserData {
        ActorUserData actorData;

        ecs::Entity headTarget;
        bool onGround = false;
        ecs::Entity standingOn;
        bool noclipping = false;
        std::shared_ptr<physx::PxMaterial> material;

        CharacterControllerUserData() {}
        CharacterControllerUserData(ecs::Entity ent) : actorData(ent, ecs::PhysicsGroup::Player) {}
    };

    struct JointState {
        ecs::PhysicsJoint ecsJoint;
        physx::PxJoint *pxJoint = nullptr;
        ForceConstraint *forceConstraint = nullptr;
        NoClipConstraint *noclipConstraint = nullptr;
    };

    class PhysxManager : public RegisteredThread {
    public:
        PhysxManager(LockFreeEventQueue<ecs::Event> &windowInputQueue);
        virtual ~PhysxManager() override;

        void StartThread(bool startPaused = false);

    private:
        void PreFrame() override;
        void Frame() override;

        physx::PxRigidActor *CreateActor(ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree, ecs::Physics>> lock,
            const ecs::Entity &e);
        size_t UpdateShapes(ecs::Lock<ecs::Read<ecs::Name, ecs::Physics>> lock,
            const ecs::Entity &owner,
            const ecs::Entity &actorEnt,
            physx::PxRigidActor *actor,
            const ecs::Transform &offset);
        void UpdateActor(ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree, ecs::Physics, ecs::SceneProperties>> lock,
            const ecs::Entity &e);
        void RemoveActor(physx::PxRigidActor *actor);
        void SetCollisionGroup(physx::PxRigidActor *actor, ecs::PhysicsGroup group);
        void SetCollisionGroup(physx::PxShape *shape, ecs::PhysicsGroup group);

    private:
        void CreatePhysxScene();
        void DestroyPhysxScene();
        void UpdateDebugLines(ecs::Lock<ecs::Write<ecs::LaserLine>> lock) const;
        void RegisterDebugCommands();

        AsyncPtr<ConvexHullSet> LoadConvexHullSet(AsyncPtr<Gltf> model, AsyncPtr<HullSettings> settings);

        physx::PxGeometryHolder GeometryFromShape(const ecs::PhysicsShape &shape,
            glm::vec3 parentScale = glm::vec3(1)) const;

        std::atomic_bool simulate = false;
        std::atomic_bool exiting = false;
        std::vector<uint8_t> scratchBlock;

        LockFreeEventQueue<ecs::Event> &windowInputQueue;

        SceneManager &scenes;
        CFuncCollection funcs;

        SimulationCallbackHandler simulationCallback;
        std::shared_ptr<physx::PxScene> scene;
        std::shared_ptr<physx::PxControllerManager> controllerManager; // Must be deconstructed before scene

        physx::PxFoundation *pxFoundation = nullptr;
        physx::PxPhysics *pxPhysics = nullptr;
        physx::PxDefaultCpuDispatcher *dispatcher = nullptr;
        physx::PxDefaultErrorCallback defaultErrorCallback;
        physx::PxDefaultAllocator defaultAllocatorCallback;
        physx::PxCooking *pxCooking = nullptr;
        physx::PxSerializationRegistry *pxSerialization = nullptr;

        physx::PxPvd *pxPvd = nullptr;
#ifndef SP_PACKAGE_RELEASE
        physx::PxPvdTransport *pxPvdTransport = nullptr;
#endif

        ecs::ComponentObserver<ecs::Physics> physicsObserver;
        ecs::EntityRef debugLineEntity = ecs::Name("physx", "debug_lines");

        CharacterControlSystem characterControlSystem;
        ConstraintSystem constraintSystem;
        PhysicsQuerySystem physicsQuerySystem;
        LaserSystem laserSystem;
        TriggerSystem triggerSystem;
        AnimationSystem animationSystem;

        EntityMap<physx::PxRigidActor *> actors, subActors;
        EntityMap<physx::PxController *> controllers;

        EntityMap<vector<JointState>> joints;

        std::mutex cacheMutex;
        PreservingMap<string, Async<ConvexHullSet>> cache;
        DispatchQueue workQueue;

        struct TransformCacheEntry {
            ecs::Entity parent;
            ecs::Transform pose;
            int dirty = -1;
        };
        EntityMap<TransformCacheEntry> transformCache;

        friend class CharacterControlSystem;
        friend class ConstraintSystem;
        friend class PhysicsQuerySystem;
        friend class LaserSystem;
    };
} // namespace sp
