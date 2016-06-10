#ifndef SP_PHYSXMANAGER_H
#define SP_PHYSXMANAGER_H

#include "Common.hh"
#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <extensions/PxDefaultAllocator.h>

namespace sp
{
	class PhysxManager
	{
	public:
		PhysxManager ();
		~PhysxManager () {};
		physx::PxRigidActor* CreateActor ();
		void Frame(double timeStep);
	private:
		void CreatePhysxScene ();
		void DestroyPhysxScene ();

		physx::PxFoundation* pxFoundation;
		physx::PxPhysics* physics;
		physx::PxDefaultErrorCallback defaultErrorCallback;
		physx::PxDefaultAllocator defaultAllocatorCallback;

		physx::PxScene* scene;
	};
}

#endif