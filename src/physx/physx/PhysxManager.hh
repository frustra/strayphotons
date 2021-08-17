#pragma once

#include "ConvexHull.hh"
#include "core/CFunc.hh"
#include "core/Common.hh"
#include "ecs/Ecs.hh"

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

    class PhysxManager {
        typedef std::list<PhysxConstraint> ConstraintList;

    public:
        PhysxManager();
        ~PhysxManager();

        void StartThread();
        void StartSimulation();
        void StopSimulation();

        bool MoveController(ecs::Lock<ecs::Write<ecs::PhysicsState>> lock,
                            physx::PxController *controller,
                            double dt,
                            physx::PxVec3 displacement);

        void EnableCollisions(ecs::Lock<ecs::Write<ecs::PhysicsState>> lock, physx::PxRigidActor *actor, bool enabled);

        void CreateConstraint(ecs::Lock<> lock,
                              Tecs::Entity parent,
                              physx::PxRigidDynamic *child,
                              physx::PxVec3 offset,
                              physx::PxQuat rotationOffset);
        void RotateConstraint(Tecs::Entity parent, physx::PxRigidDynamic *child, physx::PxVec3 rotation);
        void RemoveConstraint(Tecs::Entity parent, physx::PxRigidDynamic *child);

        bool RaycastQuery(ecs::Lock<ecs::Read<ecs::HumanController>, ecs::Write<ecs::PhysicsState>> lock,
                          Tecs::Entity entity,
                          glm::vec3 origin,
                          glm::vec3 dir,
                          const float distance,
                          physx::PxRaycastBuffer &hit);

    private:
        void AsyncFrame();

        void RemoveConstraints(physx::PxRigidDynamic *child);

        ConvexHullSet *GetCachedConvexHulls(std::string name);

        void UpdateActor(ecs::Lock<ecs::Write<ecs::Physics, ecs::PhysicsState>, ecs::Read<ecs::Transform>> lock,
                         Tecs::Entity &e);
        void RemoveActor(ecs::Lock<ecs::Write<ecs::PhysicsState>> lock, physx::PxRigidActor *actor);

        void UpdateController(
            ecs::Lock<ecs::Write<ecs::HumanController, ecs::PhysicsState>, ecs::Read<ecs::Transform>> lock,
            Tecs::Entity &e);

        bool SweepQuery(ecs::Lock<ecs::Write<ecs::PhysicsState>> lock,
                        physx::PxRigidDynamic *actor,
                        const physx::PxVec3 dir,
                        const float distance);

        /**
         * Checks scene for an overlapping hit in the shape
         * Will return true if a hit is found and false otherwise
         */
        bool OverlapQuery(ecs::Lock<ecs::Write<ecs::PhysicsState>> lock,
                          physx::PxRigidDynamic *actor,
                          physx::PxVec3 translation,
                          physx::PxOverlapBuffer &hit);

        /**
         * Translates a kinematic @actor by @transform.
         * Throws a runtime_error if @actor is not kinematic
         */
        void Translate(ecs::Lock<ecs::Write<ecs::PhysicsState>> lock,
                       physx::PxRigidDynamic *actor,
                       const physx::PxVec3 &transform);

        void ToggleDebug(ecs::Lock<ecs::Write<ecs::PhysicsState>> lock, bool enabled);
        bool IsDebugEnabled() const;

    private:
        void CreatePhysxScene();
        void DestroyPhysxScene();
        void CacheDebugLines();

        ConvexHullSet *BuildConvexHulls(Model *model, bool decomposeHull);
        ConvexHullSet *LoadCollisionCache(Model *model, bool decomposeHull);
        void SaveCollisionCache(Model *model, ConvexHullSet *set, bool decomposeHull);

        std::atomic_bool simulate = false;
        std::atomic_bool exiting = false;

        physx::PxFoundation *pxFoundation = nullptr;
        physx::PxPhysics *pxPhysics = nullptr;
        physx::PxDefaultCpuDispatcher *dispatcher = nullptr;
        physx::PxDefaultErrorCallback defaultErrorCallback;
        physx::PxDefaultAllocator defaultAllocatorCallback;
        physx::PxCooking *pxCooking = nullptr;

#if !defined(PACKAGE_RELEASE)
        physx::PxPvd *pxPvd = nullptr;
        physx::PxPvdTransport *pxPvdTransport = nullptr;
#endif

        vector<uint8_t> scratchBlock;
        bool debug = false;

        std::thread thread;

        ecs::Observer<ecs::Removed<ecs::Physics>> physicsRemoval;
        ecs::Observer<ecs::Removed<ecs::HumanController>> humanControllerRemoval;

        ConstraintList constraints;

        std::unordered_map<string, ConvexHullSet *> cache;

        CFuncCollection funcs;
    };
} // namespace sp
