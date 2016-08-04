#include "physx/PhysxManager.hh"
#include "core/Logging.hh"
#include "assets/Model.hh"

namespace sp
{
	using namespace physx;

	PhysxManager::PhysxManager()
	{
		Logf("PhysX %d.%d.%d starting up", PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX);

		PxTolerancesScale scale;

		pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
		physics = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, scale);
		Assert(physics, "PxCreatePhysics");

		pxCooking = PxCreateCooking(PX_PHYSICS_VERSION, *pxFoundation, PxCookingParams(scale));
		Assert(pxCooking, "PxCreateCooking");

		CreatePhysxScene();
	}

	PhysxManager::~PhysxManager()
	{
		DestroyPhysxScene();

		pxCooking->release();
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
		PxRigidStatic *groundPlane = PxCreatePlane(*physics, PxPlane(0.f, 1.f, 0.f, 1.03f), *groundMat);
		scene->addActor(*groundPlane);
	}

	void PhysxManager::DestroyPhysxScene()
	{
		scene->release();
		dispatcher->release();
	}

	PxRigidActor *PhysxManager::CreateActor(shared_ptr<Model> model, PxTransform transform, bool dynamic)
	{
		Logf("%d, %d, %d", transform.p.x, transform.p.y, transform.p.z);
		Logf("%d, %d, %d", transform.p.x, transform.p.y, transform.p.z);

		PxRigidActor *actor;

		if (dynamic)
			actor = physics->createRigidDynamic(transform);
		else
			actor = physics->createRigidStatic(transform);

		PxMaterial *mat = physics->createMaterial(0.6f, 0.5f, 0.0f);

		for (auto prim : model->primitives)
		{
			auto posAttrib = prim->attributes[0];
			Assert(posAttrib.componentCount == 3);
			Assert(posAttrib.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

			// Build convex hull
			auto buffer = model->GetScene()->buffers[posAttrib.bufferName];

			PxConvexMeshDesc convexDesc;
			convexDesc.points.count = posAttrib.components;
			convexDesc.points.stride = posAttrib.byteStride;
			convexDesc.points.data = (PxVec3 *)(buffer.data.data() + posAttrib.byteOffset);
			convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

			PxDefaultMemoryOutputStream buf;
			PxConvexMeshCookingResult::Enum result;

			if (!pxCooking->cookConvexMesh(convexDesc, buf, &result))
			{
				Errorf("Failed to cook PhysX hull for %s", model->name);
				return nullptr;
			}

			PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
			PxConvexMesh *hull = physics->createConvexMesh(input);

			actor->createShape(PxConvexMeshGeometry(hull), *mat);
		}

		if (dynamic)
			PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidDynamic *>(actor), 1.0f);

		scene->addActor(*actor);
		return actor;
	}
}
