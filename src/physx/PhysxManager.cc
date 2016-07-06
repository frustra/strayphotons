#include "physx/PhysxManager.hh"
#include "core/Logging.hh"

namespace sp
{
	PhysxManager::PhysxManager()
	{
		Logf("PhysX starting up");
		pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
		physics = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, physx::PxTolerancesScale() );
		if (physics == NULL)
		{
			Errorf("Error creating PhysX device.");
		}
		CreatePhysxScene();
	}

	void PhysxManager::Frame(double timeStep)
	{
		scene->simulate(timeStep);
		scene->fetchResults(true);
	}

	void PhysxManager::CreatePhysxScene()
	{
		physx::PxSceneDesc sceneDesc(physics->getTolerancesScale());

		sceneDesc.gravity = physx::PxVec3(0, -1, 0);

		if (!sceneDesc.filterShader)
			sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

		physx::PxDefaultCpuDispatcher *mCpuDispatcher = physx::PxDefaultCpuDispatcherCreate(1);
		sceneDesc.cpuDispatcher = mCpuDispatcher;

		scene = physics->createScene(sceneDesc);
		Assert(scene, "creating PhysX scene");
	}

	physx::PxRigidActor *PhysxManager::CreateActor()
	{
		physx::PxTransform position (0, 0, -4);
		physx::PxRigidActor *capsule = physics->createRigidDynamic(position);
		scene->addActor(*capsule);
		return capsule;
	}
}