#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Physics.hh"
#include "physx/AnimationSystem.hh"
#include "physx/CharacterControlSystem.hh"
#include "physx/ConstraintSystem.hh"
#include "physx/ConvexHull.hh"
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
    class Model;
    class PhysxManager;

    extern CVar<float> CVarGravity;

    struct ActorUserData {
        Tecs::Entity entity;
        uint32_t transformChangeNumber = 0;
        ecs::PhysicsGroup currentPhysicsGroup;

        ActorUserData() {}
        ActorUserData(Tecs::Entity ent, uint32_t changeNumber, ecs::PhysicsGroup group)
            : entity(ent), transformChangeNumber(changeNumber), currentPhysicsGroup(group) {}
    };

    struct CharacterControllerUserData {
        ActorUserData actorData;

        bool onGround = false;
        bool noclipping = false;
        glm::vec3 velocity = glm::vec3(0);

        CharacterControllerUserData() {}
        CharacterControllerUserData(Tecs::Entity ent, uint32_t changeNumber)
            : actorData(ent, changeNumber, ecs::PhysicsGroup::Player) {}
    };

    class ControllerHitReport : public physx::PxUserControllerHitReport {
    public:
        ControllerHitReport() {}
        void onShapeHit(const physx::PxControllerShapeHit &hit);
        void onControllerHit(const physx::PxControllersHit &hit) {}
        void onObstacleHit(const physx::PxControllerObstacleHit &hit) {}
    };

    class PhysxManager : public RegisteredThread {
    public:
        PhysxManager();
        virtual ~PhysxManager() override;

        void SetCollisionGroup(physx::PxRigidActor *actor, ecs::PhysicsGroup group);

    private:
        void Frame() override;

        ConvexHullSet *GetCachedConvexHulls(std::string name);

        void CreateActor(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::Physics>> lock, Tecs::Entity &e);
        void UpdateActor(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::Physics>> lock, Tecs::Entity &e);
        void RemoveActor(physx::PxRigidActor *actor);

        bool SweepQuery(physx::PxRigidDynamic *actor, physx::PxVec3 dir, float distance, physx::PxSweepBuffer &hit);

        /**
         * Checks scene for an overlapping hit in the shape
         * Will return true if a hit is found and false otherwise
         */
        bool OverlapQuery(physx::PxRigidDynamic *actor, physx::PxVec3 translation, physx::PxOverlapBuffer &hit);

    private:
        void CreatePhysxScene();
        void DestroyPhysxScene();
        void CacheDebugLines();

        ConvexHullSet *BuildConvexHulls(const Model &model, bool decomposeHull);
        ConvexHullSet *LoadCollisionCache(const Model &model, bool decomposeHull);
        void SaveCollisionCache(const Model &model, ConvexHullSet *set, bool decomposeHull);

        std::atomic_bool simulate = false;
        std::atomic_bool exiting = false;
        vector<uint8_t> scratchBlock;

        std::shared_ptr<physx::PxScene> scene;
        std::unique_ptr<ControllerHitReport> controllerHitReporter;
        std::shared_ptr<physx::PxControllerManager> controllerManager; // Must be deconstructed before scene

        physx::PxFoundation *pxFoundation = nullptr;
        physx::PxPhysics *pxPhysics = nullptr;
        physx::PxDefaultCpuDispatcher *dispatcher = nullptr;
        physx::PxDefaultErrorCallback defaultErrorCallback;
        physx::PxDefaultAllocator defaultAllocatorCallback;
        physx::PxCooking *pxCooking = nullptr;

        physx::PxPvd *pxPvd = nullptr;
#ifndef SP_PACKAGE_RELEASE
        physx::PxPvdTransport *pxPvdTransport = nullptr;
#endif

        ecs::ComponentObserver<ecs::Physics> physicsObserver;

        CharacterControlSystem characterControlSystem;
        ConstraintSystem constraintSystem;
        PhysicsQuerySystem physicsQuerySystem;
        LaserSystem laserSystem;
        TriggerSystem triggerSystem;
        AnimationSystem animationSystem;

        std::unordered_map<string, ConvexHullSet *> cache;

        friend class CharacterControlSystem;
        friend class ConstraintSystem;
        friend class PhysicsQuerySystem;
        friend class LaserSystem;
    };
} // namespace sp
