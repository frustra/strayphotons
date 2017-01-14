#ifndef SP_PHYSXMANAGER_H
#define SP_PHYSXMANAGER_H

#include "Common.hh"
#include "ConvexHull.hh"

#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <extensions/PxDefaultAllocator.h>
#include <unordered_map>

namespace sp
{
	class Model;

	class PhysxManager
	{
	public:
		PhysxManager();
		~PhysxManager();

		void Frame(double timeStep);
		void StartThread();
		void StartSimulation();
		void StopSimulation();
		void ReleaseControllers();
		void Lock();
		void Unlock();
		void ReadLock();
		void ReadUnlock();

		ConvexHullSet *GetCollisionMesh(shared_ptr<Model> model);

		struct ActorDesc
		{
			physx::PxTransform transform;
			physx::PxMeshScale scale;
			bool dynamic = true;
			//bool mergePrimitives = true;
		};

		physx::PxRigidActor *CreateActor(shared_ptr<Model> model, ActorDesc desc);
		physx::PxController *CreateController(physx::PxVec3 pos, float radius, float height, float density);

	private:
		void CreatePhysxScene();
		void DestroyPhysxScene();

		ConvexHullSet *BuildConvexHulls(Model *model);

		physx::PxFoundation *pxFoundation;
		physx::PxPhysics *physics;
		physx::PxDefaultCpuDispatcher *dispatcher;
		physx::PxDefaultErrorCallback defaultErrorCallback;
		physx::PxDefaultAllocator defaultAllocatorCallback;
		physx::PxCooking *pxCooking;
		physx::PxControllerManager* manager; 

		physx::PxScene *scene;
		bool simulate = false, resultsPending = false;

		std::unordered_map<string, ConvexHullSet *> cache;
	};
}

#endif
