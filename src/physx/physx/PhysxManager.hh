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
        ControllerHitReport(PhysxManager *manager) : manager(manager) {}
        void onShapeHit(const physx::PxControllerShapeHit &hit);
        void onControllerHit(const physx::PxControllersHit &hit) {}
        void onObstacleHit(const physx::PxControllerObstacleHit &hit) {}

    private:
        PhysxManager *manager;
    };

    enum PhysxCollisionGroup { HELD_OBJECT = 1, PLAYER = 2, WORLD = 3, NOCLIP = 4 };

    class PhysxManager {
        typedef std::list<PhysxConstraint> ConstraintList;

    public:
        PhysxManager(ecs::ECS &ecs);
        ~PhysxManager();

        void StartThread();
        void StartSimulation();
        void StopSimulation();

        physx::PxCapsuleController *CreateController(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock,
                                                     physx::PxVec3 pos,
                                                     float radius,
                                                     float height,
                                                     float density);

        bool MoveController(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock,
                            physx::PxController *controller,
                            double dt,
                            physx::PxVec3 displacement);
        void TeleportController(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock,
                                physx::PxController *controller,
                                physx::PxExtendedVec3 position);

        /**
         * height should not include the height of top and bottom
         * radiuses for capsule controllers
         */
        void ResizeController(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock,
                              physx::PxController *controller,
                              const float height,
                              bool fromTop);

        void EnableCollisions(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock, physx::PxRigidActor *actor, bool enabled);

        void CreateConstraint(ecs::Lock<> lock,
                              Tecs::Entity parent,
                              physx::PxRigidDynamic *child,
                              physx::PxVec3 offset,
                              physx::PxQuat rotationOffset);
        void RotateConstraint(Tecs::Entity parent, physx::PxRigidDynamic *child, physx::PxVec3 rotation);
        void RemoveConstraint(Tecs::Entity parent, physx::PxRigidDynamic *child);

        bool RaycastQuery(ecs::Lock<ecs::Read<ecs::HumanController>, ecs::Write<ecs::PhysicsScene>> lock,
                          Tecs::Entity entity,
                          glm::vec3 origin,
                          glm::vec3 dir,
                          const float distance,
                          physx::PxRaycastBuffer &hit);

        /**
         * Checks scene for an overlapping hit in the shape
         * Will return true if a hit is found and false otherwise
         */
        bool OverlapQuery(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock,
                          physx::PxRigidDynamic *actor,
                          physx::PxVec3 translation,
                          physx::PxOverlapBuffer &hit);

    private:
        void AsyncFrame();

        void RemoveConstraints(physx::PxRigidDynamic *child);

        ConvexHullSet *GetCachedConvexHulls(std::string name);

        void UpdateActor(ecs::Lock<ecs::Write<ecs::Physics, ecs::PhysicsScene>, ecs::Read<ecs::Transform>> lock,
                         Tecs::Entity &e);
        void RemoveActor(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock, physx::PxRigidActor *actor);

        void RemoveController(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock, physx::PxController *controller);

        /**
         * Get the Entity associated with this actor.
         * Returns the NULL Entity Id if one doesn't exist.
         */
        ecs::Entity::Id GetEntityId(const physx::PxActor &actor) const;

        bool SweepQuery(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock,
                        physx::PxRigidDynamic *actor,
                        const physx::PxVec3 dir,
                        const float distance);

        /**
         * Translates a kinematic @actor by @transform.
         * Throws a runtime_error if @actor is not kinematic
         */
        void Translate(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock,
                       physx::PxRigidDynamic *actor,
                       const physx::PxVec3 &transform);

        void ToggleDebug(ecs::Lock<ecs::Write<ecs::PhysicsScene>> lock, bool enabled);
        bool IsDebugEnabled() const;

    private:
        physx::PxScene *CreatePhysxScene();
        void DestroyPhysxScene();
        void CacheDebugLines();

        ConvexHullSet *BuildConvexHulls(Model *model, bool decomposeHull);
        ConvexHullSet *LoadCollisionCache(Model *model, bool decomposeHull);
        void SaveCollisionCache(Model *model, ConvexHullSet *set, bool decomposeHull);

        ecs::ECS &ecs;

        std::atomic_bool simulate = false;
        std::atomic_bool exiting = false;

        physx::PxFoundation *pxFoundation = nullptr;
        physx::PxPhysics *physics = nullptr;
        physx::PxDefaultCpuDispatcher *dispatcher = nullptr;
        physx::PxDefaultErrorCallback defaultErrorCallback;
        physx::PxDefaultAllocator defaultAllocatorCallback;
        physx::PxCooking *pxCooking = nullptr;
        physx::PxControllerManager *manager = nullptr;

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
