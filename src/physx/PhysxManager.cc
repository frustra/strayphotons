#include "physx/PhysxManager.hh"
#include "core/Logging.hh"

namespace sp
{
	PhysxManager::PhysxManager ()
	{
		Logf("PhysX starting up");
		pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
		physics = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, physx::PxTolerancesScale() );
		if(physics == NULL) {
			Errorf("Error creating PhysX device.");
		}
		CreatePhysxScene();
	}

	void PhysxManager::Frame () {
		scene->simulate(timeStep);

	   	while(!scene->fetchResults() ) {

	   	}  
	}

	void PhysxManager::CreatePhysxScene () {
  		physx::PxSceneDesc sceneDesc(physics->getTolerancesScale());

  		sceneDesc.gravity = physx::PxVec3(0, -9.81, 0);
	    if(!sceneDesc.filterShader) {
	        sceneDesc.filterShader  = physx::PxDefaultSimulationFilterShader;
	    }

    	physx::PxDefaultCpuDispatcher* mCpuDispatcher = physx::PxDefaultCpuDispatcherCreate(1);
        sceneDesc.cpuDispatcher = mCpuDispatcher;

		scene = physics->createScene(sceneDesc);
		if (!scene) {
			Errorf("Error creating PhysX Scene");
		}

		//Create a bottom floor plane
	 	physx::PxMaterial* mMaterial = physics->createMaterial(0.5,0.5,0.5);
		physx::PxReal d = 0.0f;  
		physx::PxTransform pose = physx::PxTransform(physx::PxVec3(0.0f, 0, 0.0f),physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f)));
		physx::PxRigidStatic* plane = physics->createRigidStatic(pose);
		if (!plane) {
			Errorf("create plane failed!");
		}
		physx::PxShape* shape = plane->createShape(physx::PxPlaneGeometry(), *mMaterial);
		if (!shape) {
			Errorf("create shape failed!");
		}
		scene->addActor(*plane);
	}

	physx::PxRigidActor* PhysxManager::CreateActor () {
		physx::PxTransform position (0,100,0);
		physx::PxRigidActor* capsule = physics->createRigidDynamic(position);
		scene->addActor(*capsule);
		return capsule;
	}
}