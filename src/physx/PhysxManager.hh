#ifndef SP_PHYSXMANAGER_H
#define SP_PHYSXMANAGER_H

#include "Common.hh"
#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <extensions/PxDefaultAllocator.h>

namespace sp
{
	class Model;

	class PhysxManager
	{
	public:
		PhysxManager();
		~PhysxManager();
		physx::PxRigidActor *CreateActor(shared_ptr<Model> model, physx::PxTransform transform = physx::PxTransform(), physx::PxMeshScale scale = physx::PxMeshScale(), bool dynamic = true);
		void Frame(double timeStep);
	private:
		void CreatePhysxScene();
		void DestroyPhysxScene();

		physx::PxFoundation *pxFoundation;
		physx::PxPhysics *physics;
		physx::PxDefaultCpuDispatcher *dispatcher;
		physx::PxDefaultErrorCallback defaultErrorCallback;
		physx::PxDefaultAllocator defaultAllocatorCallback;
		physx::PxCooking *pxCooking;

		physx::PxScene *scene;
	};
}

#endif
