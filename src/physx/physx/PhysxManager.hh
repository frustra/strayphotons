#pragma once

#include "ConvexHull.hh"
#include "core/CFunc.hh"
#include "core/Common.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "physx/HumanControlSystem.hh"

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

    struct PhysxConstraint {
        Tecs::Entity parent;
        physx::PxRigidDynamic *child;
        physx::PxVec3 offset, rotation;
        physx::PxQuat rotationOffset;
    };

    class ControllerHitReport : public physx::PxUserControllerHitReport {
    public:
        ControllerHitReport() {}
        void onShapeHit(const physx::PxControllerShapeHit &hit);
        void onControllerHit(const physx::PxControllersHit &hit) {}
        void onObstacleHit(const physx::PxControllerObstacleHit &hit) {}
    };

    enum PhysxCollisionGroup { HELD_OBJECT = 1, PLAYER = 2, WORLD = 3, NOCLIP = 4 };

    class PhysxManager : public RegisteredThread {
        typedef std::list<PhysxConstraint> ConstraintList;

    public:
        PhysxManager();
        virtual ~PhysxManager() override;

        bool MoveController(physx::PxController *controller, double dt, physx::PxVec3 displacement);

        void EnableCollisions(physx::PxRigidActor *actor, bool enabled);

        void CreateConstraint(ecs::Lock<> lock,
                              Tecs::Entity parent,
                              physx::PxRigidDynamic *child,
                              physx::PxVec3 offset,
                              physx::PxQuat rotationOffset);
        void RotateConstraint(Tecs::Entity parent, physx::PxRigidDynamic *child, physx::PxVec3 rotation);
        void RemoveConstraint(Tecs::Entity parent, physx::PxRigidDynamic *child);

        bool RaycastQuery(ecs::Lock<ecs::Read<ecs::HumanController>> lock,
                          Tecs::Entity entity,
                          glm::vec3 origin,
                          glm::vec3 dir,
                          const float distance,
                          physx::PxRaycastBuffer &hit);

    private:
        void Frame() override;

        void RemoveConstraints(physx::PxRigidDynamic *child);

        ConvexHullSet *GetCachedConvexHulls(std::string name);

        void UpdateActor(ecs::Lock<ecs::Write<ecs::Physics, ecs::Transform>> lock, Tecs::Entity &e);
        void RemoveActor(physx::PxRigidActor *actor);

        void UpdateController(ecs::Lock<ecs::Write<ecs::HumanController, ecs::Transform>> lock, Tecs::Entity &e);

        bool SweepQuery(physx::PxRigidDynamic *actor, const physx::PxVec3 dir, const float distance);

        /**
         * Checks scene for an overlapping hit in the shape
         * Will return true if a hit is found and false otherwise
         */
        bool OverlapQuery(physx::PxRigidDynamic *actor, physx::PxVec3 translation, physx::PxOverlapBuffer &hit);

        /**
         * Translates a kinematic @actor by @transform.
         */
        void Translate(physx::PxRigidDynamic *actor, const physx::PxVec3 &transform);

        void ToggleDebug(bool enabled);
        bool IsDebugEnabled() const;

    private:
        void CreatePhysxScene();
        void DestroyPhysxScene();
        void CacheDebugLines();

        ConvexHullSet *BuildConvexHulls(const Model &model, bool decomposeHull);
        ConvexHullSet *LoadCollisionCache(const Model &model, bool decomposeHull);
        void SaveCollisionCache(const Model &model, ConvexHullSet *set, bool decomposeHull);

        std::atomic_bool simulate = false;
        std::atomic_bool exiting = false;

        std::shared_ptr<physx::PxControllerManager> controllerManager; // Must be deconstructed before scene
        std::shared_ptr<physx::PxScene> scene;

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

        vector<uint8_t> scratchBlock;
        bool debug = false;

        ecs::Observer<ecs::Removed<ecs::Physics>> physicsRemoval;
        ecs::Observer<ecs::Removed<ecs::HumanController>> humanControllerRemoval;

        HumanControlSystem humanControlSystem;

        ConstraintList constraints;

        std::unordered_map<string, ConvexHullSet *> cache;

        CFuncCollection funcs;
    };
} // namespace sp
