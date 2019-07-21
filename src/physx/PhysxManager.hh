#pragma once

#include "Common.hh"
#include "ConvexHull.hh"
#include "PhysxActorDesc.hh"
#include "core/CFunc.hh"
#include "threading/MutexedVector.hh"

#include <Ecs.hh>

#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <extensions/PxDefaultAllocator.h>
#include <unordered_map>
#include <thread>
#include <functional>

namespace sp
{
	class Model;
	class PhysxManager;

	struct PhysxConstraint
	{
		ecs::Entity parent;
		physx::PxRigidDynamic *child;
		physx::PxVec3 offset, rotation;
		physx::PxQuat rotationOffset;
	};

	class ControllerHitReport : public physx::PxUserControllerHitReport
	{
	public:
		ControllerHitReport(PhysxManager *manager) : manager(manager) {}
		void onShapeHit(const physx::PxControllerShapeHit &hit);
		void onControllerHit(const physx::PxControllersHit &hit) {}
		void onObstacleHit(const physx::PxControllerObstacleHit &hit) {}
	private:
		PhysxManager *manager;
	};

	class PhysxManager
	{
		typedef std::list<PhysxConstraint> ConstraintList;

	public:
		PhysxManager();
		~PhysxManager();

		void Frame(double timeStep);
		bool LogicFrame(ecs::EntityManager &manager);
		void StartThread();
		void StartSimulation();
		void StopSimulation();
		void Lock();
		void Unlock();
		void ReadLock();
		void ReadUnlock();

		void CreateConstraint(ecs::Entity parent, physx::PxRigidDynamic *child, physx::PxVec3 offset, physx::PxQuat rotationOffset);
		void RotateConstraint(ecs::Entity parent, physx::PxRigidDynamic *child, physx::PxVec3 rotation);
		void RemoveConstraint(ecs::Entity parent, physx::PxRigidDynamic *child);
		void RemoveConstraints(physx::PxRigidDynamic *child);

		ConvexHullSet *GetCachedConvexHulls(std::string name);

		/**
		 * Create an actor and bind the entity's Id to the actor's userData
		 */
		physx::PxRigidActor *CreateActor(shared_ptr<Model> model, PhysxActorDesc desc, const ecs::Entity &entity);

		void RemoveActor(physx::PxRigidActor *actor);

		physx::PxCapsuleController *CreateController(physx::PxVec3 pos, float radius, float height, float density);

		/**
		 * Get the Entity associated with this actor.
		 * Returns the NULL Entity Id if one doesn't exist.
		 */
		ecs::Entity::Id GetEntityId(const physx::PxActor &actor) const;

		bool MoveController(physx::PxController *controller, double dt, physx::PxVec3 displacement);
		void TeleportController(physx::PxController *controller, physx::PxExtendedVec3 position);

		/**
		 * height should not include the height of top and bottom
		 * radiuses for capsule controllers
		 */
		void ResizeController(physx::PxController *controller, const float height, bool fromTop);

		void RemoveController(physx::PxController *controller);

		float GetCapsuleHeight(physx::PxCapsuleController *controller);

		bool RaycastQuery(
			ecs::Entity &entity,
			const physx::PxVec3 origin,
			const physx::PxVec3 dir,
			const float distance,
			physx::PxRaycastBuffer &hit);

		bool SweepQuery(
			physx::PxRigidDynamic *actor,
			const physx::PxVec3 dir,
			const float distance);

		/**
		* Checks scene for an overlapping hit in the shape
		* Will return true if a hit is found and false otherwise
		*/
		bool OverlapQuery(
			physx::PxRigidDynamic *actor,
			physx::PxVec3 translation,
			physx::PxOverlapBuffer& hit);

		/**
		 * Translates a kinematic @actor by @transform.
		 * Throws a runtime_error if @actor is not kinematic
		 */
		void Translate(
			physx::PxRigidDynamic *actor,
			const physx::PxVec3 &transform);

		/**
		 * Collisions between this actor's shapes and other physx objects
		 * will be enabled (default).
		 */
		void EnableCollisions(physx::PxRigidActor *actor);

		/**
		 * Collisions between this actor's shapes and other physx objects
		 * will be disabled.
		 */
		void DisableCollisions(physx::PxRigidActor *actor);

		/**
		 * Enable or disable collisions for an actor.
		 */
		void ToggleCollisions(physx::PxRigidActor *actor, bool enabled);

		void ToggleDebug(bool enabled);
		bool IsDebugEnabled() const;

		/**
		 * Get the lines for the bounds of all physx objects
		 */
		MutexedVector<physx::PxDebugLine> GetDebugLines();

	private:
		void CreatePhysxScene();
		void DestroyPhysxScene();
		void CacheDebugLines();

		ConvexHullSet *BuildConvexHulls(Model *model, bool decomposeHull);
		ConvexHullSet *LoadCollisionCache(Model *model, bool decomposeHull);
		void SaveCollisionCache(Model *model, ConvexHullSet *set, bool decomposeHull);

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

		physx::PxScene *scene = nullptr;
		bool simulate = false, exiting = false, resultsPending = false;
		bool debug = false;

		std::thread thread;

		ConstraintList constraints;

		std::unordered_map<string, ConvexHullSet *> cache;

		vector<physx::PxDebugLine> debugLines;
		std::mutex debugLinesMutex;

		CFuncCollection funcs;
	};
}
