#pragma once

#include "assets/Async.hh"
#include "console/CFunc.hh"
#include "cooking/ConvexHull.hh"
#include "core/Common.hh"
#include "core/DispatchQueue.hh"
#include "core/EntityMap.hh"
#include "core/Logging.hh"
#include "core/PreservingMap.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Transform.h"
#include "physx/AnimationSystem.hh"
#include "physx/CharacterControlSystem.hh"
#include "physx/ConstraintSystem.hh"
#include "physx/LaserSystem.hh"
#include "physx/PhysicsQuerySystem.hh"
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

    struct ActorUserData {
        ecs::Entity entity;
        ecs::Transform pose;
        glm::vec3 scale = glm::vec3(1);
        glm::vec3 velocity = glm::vec3(0);
        glm::vec3 gravity = glm::vec3(0);
        float angularDamping = 0.0f;
        float linearDamping = 0.0f;
        ecs::PhysicsGroup physicsGroup = ecs::PhysicsGroup::NoClip;
        std::vector<std::pair<ecs::PhysicsShape, size_t>> shapeIndexes;
        std::shared_ptr<const ConvexHullSet> shapeCache;
        std::shared_ptr<physx::PxMaterial> material;

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

        CharacterControllerUserData() {}
        CharacterControllerUserData(ecs::Entity ent) : actorData(ent, ecs::PhysicsGroup::Player) {}
    };

    struct JointState {
        ecs::PhysicsJoint ecsJoint;
        physx::PxJoint *pxJoint = nullptr;
        ForceConstraint *forceConstraint = nullptr;
    };

    class PhysxManager : public RegisteredThread {
    public:
        PhysxManager(bool stepMode);
        virtual ~PhysxManager() override;

        void SetCollisionGroup(physx::PxRigidActor *actor, ecs::PhysicsGroup group);

    private:
        void PreFrame() override;
        void Frame() override;

        physx::PxRigidActor *CreateActor(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::Physics>> lock, ecs::Entity &e);
        void UpdateActor(ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree, ecs::Physics, ecs::SceneInfo>> lock,
            ecs::Entity &e);
        void RemoveActor(physx::PxRigidActor *actor);

    private:
        void CreatePhysxScene();
        void DestroyPhysxScene();
        void CacheDebugLines();

        AsyncPtr<ConvexHullSet> LoadConvexHullSet(AsyncPtr<Gltf> model, AsyncPtr<HullSettings> settings);

        physx::PxGeometryHolder GeometryFromShape(const ecs::PhysicsShape &shape, glm::vec3 parentScale = glm::vec3(1));

        std::atomic_bool simulate = false;
        std::atomic_bool exiting = false;
        std::vector<uint8_t> scratchBlock;

        SceneManager &scenes;
        CFuncCollection funcs;

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

        EntityMap<physx::PxRigidActor *> actors;
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
