#include "physx/PhysxManager.hh"
#include "core/Logging.hh"

namespace sp
{
	using namespace physx;

	PhysxManager::PhysxManager()
	{
		Logf("PhysX %d.%d.%d starting up", PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX);

		pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
		physics = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, PxTolerancesScale());
		Assert(physics != nullptr, "creating PhysX context");

		CreatePhysxScene();
	}

	PhysxManager::~PhysxManager()
	{
		DestroyPhysxScene();

		physics->release();
		pxFoundation->release();
	}

	void PhysxManager::Frame(double timeStep)
	{
		scene->simulate((PxReal) timeStep);
		scene->fetchResults(true);
	}

	void PhysxManager::CreatePhysxScene()
	{
		PxSceneDesc sceneDesc(physics->getTolerancesScale());

		sceneDesc.gravity = PxVec3(0.f, -9.81f, 0.f);

		if (!sceneDesc.filterShader)
			sceneDesc.filterShader = PxDefaultSimulationFilterShader;

		dispatcher = PxDefaultCpuDispatcherCreate(1);
		sceneDesc.cpuDispatcher = dispatcher;

		scene = physics->createScene(sceneDesc);
		Assert(scene, "creating PhysX scene");

		PxMaterial *groundMat = physics->createMaterial(0.6f, 0.5f, 0.0f);
		PxRigidStatic *groundPlane = PxCreatePlane(*physics, PxPlane(0.f, 1.f, 0.f, 0.f), *groundMat);
		scene->addActor(*groundPlane);
	}

	void PhysxManager::DestroyPhysxScene()
	{
		scene->release();
		dispatcher->release();
	}

	PxRigidActor *PhysxManager::CreateActor()
	{
		PxTransform position (0, 6, -4);
		PxRigidDynamic *capsule = physics->createRigidDynamic(position);
		PxMaterial *mat = physics->createMaterial(0.6f, 0.5f, 0.0f);
		capsule->createShape(PxCapsuleGeometry(0.5f, 0.5f), *mat)->setLocalPose({ 0, 1.0f, 0 });
		PxRigidBodyExt::updateMassAndInertia(*capsule, 1.0f);
		scene->addActor(*capsule);
		return capsule;
	}
}