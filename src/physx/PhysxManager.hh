#ifndef SP_PHYSXMANAGER_H
#define SP_PHYSXMANAGER_H

#include "Common.hh"
#include "ConvexHull.hh"

#include <Ecs.hh>

#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <extensions/PxDefaultAllocator.h>
#include <unordered_map>
#include <thread>

namespace sp
{
	class Model;

	struct PhysxConstraint
	{
		ecs::Entity parent;
		physx::PxRigidDynamic* child;
		physx::PxVec3 offset;
	};

	class PhysxManager
	{
		typedef std::list<PhysxConstraint> ConstraintList;

	public:
		PhysxManager();
		~PhysxManager();

		void Frame(double timeStep);
		void StartThread();
		void StartSimulation();
		void StopSimulation();
		void Lock();
		void Unlock();
		void ReadLock();
		void ReadUnlock();

		void CreateConstraint(ecs::Entity parent, physx::PxRigidDynamic* child, physx::PxVec3 offset);

		ConvexHullSet *GetCachedConvexHulls(Model *model);

		struct ActorDesc
		{
			physx::PxTransform transform;
			physx::PxMeshScale scale;
			bool dynamic = true;
			//bool mergePrimitives = true;
		};

		physx::PxRigidActor *CreateActor(shared_ptr<Model> model, ActorDesc desc);
		void RemoveActor(physx::PxRigidActor *actor);
		physx::PxController *CreateController(physx::PxVec3 pos, float radius, float height, float density);
		void RemoveController(physx::PxController *controller);

		bool RaycastQuery(ecs::Entity& entity, const physx::PxVec3 origin, const physx::PxVec3 dir, const float distance, physx::PxRaycastBuffer& hit);

	private:
		void CreatePhysxScene();
		void DestroyPhysxScene();

		ConvexHullSet *BuildConvexHulls(Model *model);
		ConvexHullSet *LoadCollisionCache(Model *model);
		void SaveCollisionCache(Model *model, ConvexHullSet *set);

		physx::PxFoundation *pxFoundation = nullptr;
		physx::PxPhysics *physics = nullptr;
		physx::PxDefaultCpuDispatcher *dispatcher = nullptr;
		physx::PxDefaultErrorCallback defaultErrorCallback;
		physx::PxDefaultAllocator defaultAllocatorCallback;
		physx::PxCooking *pxCooking = nullptr;
		physx::PxControllerManager *manager = nullptr;

		physx::PxScene *scene = nullptr;
		bool simulate = false, exiting = false, resultsPending = false;

		std::thread thread;

		ConstraintList constraints;

		std::unordered_map<string, ConvexHullSet *> cache;
	};
}

#endif
