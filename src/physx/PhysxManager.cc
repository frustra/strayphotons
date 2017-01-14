#include <PxScene.h>

#include "physx/PhysxManager.hh"

#include "assets/Model.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"

#include <thread>

namespace sp
{
	using namespace physx;
	static CVar<float> CVarGravity("g.Gravity", -9.81f, "Force of gravity");

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
		StartThread();
		StartSimulation();
	}

	PhysxManager::~PhysxManager()
	{
		StopSimulation();
		ReleaseControllers();
		DestroyPhysxScene();

		pxCooking->release();
		physics->release();
		pxFoundation->release();
	}

	void PhysxManager::Frame(double timeStep)
	{
		bool hadResults = false;

		while (resultsPending)
		{
			if (!simulate)
			{
				return;
			}

			hadResults = true;
			Lock();

			if (scene->fetchResults())
			{
				// Lock continues to be held.
				resultsPending = false;
				break;
			}

			Unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}

		if (!hadResults)
			Lock();

		if (CVarGravity.Changed())
		{
			scene->setGravity(PxVec3(0.f, CVarGravity.Get(true), 0.f));

			vector<PxActor *> buffer(256, nullptr);
			size_t startIndex = 0;

			while (true)
			{
				size_t n = scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, &buffer[0], buffer.size(), startIndex);

				for (size_t i = 0; i < n; i++)
				{
					auto dynamicActor = static_cast<PxRigidDynamic *>(buffer[i]);
					dynamicActor->wakeUp();
				}

				if (n < buffer.size())
					break;

				startIndex += n;
			}
		}

		scene->simulate((PxReal) timeStep);
		resultsPending = true;
		Unlock();
	}

	void PhysxManager::CreatePhysxScene()
	{
		PxSceneDesc sceneDesc(physics->getTolerancesScale());

		sceneDesc.gravity = PxVec3(0.f, CVarGravity.Get(true), 0.f);

		if (!sceneDesc.filterShader)
			sceneDesc.filterShader = PxDefaultSimulationFilterShader;

		dispatcher = PxDefaultCpuDispatcherCreate(1);
		sceneDesc.cpuDispatcher = dispatcher;

		scene = physics->createScene(sceneDesc);
		Assert(scene, "creating PhysX scene");

		Lock();
		PxMaterial *groundMat = physics->createMaterial(0.6f, 0.5f, 0.0f);
		PxRigidStatic *groundPlane = PxCreatePlane(*physics, PxPlane(0.f, 1.f, 0.f, 1.03f), *groundMat);
		scene->addActor(*groundPlane);
		Unlock();
	}

	void PhysxManager::DestroyPhysxScene()
	{
		Lock();
		scene->fetchResults();
		scene->release();
		scene = nullptr;
		dispatcher->release();
		dispatcher = nullptr;
	}

	void PhysxManager::StartThread()
	{
		std::thread thread([&]
		{
			const double rate = 60.0; // frames/sec

			while (true)
			{
				double frameStart = glfwGetTime();

				if (simulate)
					Frame(1.0 / rate);

				double frameEnd = glfwGetTime();
				double sleepFor = 1.0 / rate + frameStart - frameEnd;

				std::this_thread::sleep_for(std::chrono::nanoseconds((uint64) (sleepFor * 1e9)));
			}
		});

		thread.detach();
	}

	void PhysxManager::StartSimulation()
	{
		Lock();
		simulate = true;
		Unlock();
	}

	void PhysxManager::StopSimulation()
	{
		Lock();
		simulate = false;
		Unlock();
	}

	void PhysxManager::ReleaseControllers()
	{
		Lock();
		if(manager)
		{
			manager->purgeControllers();
		}
		Unlock();
	}

	void PhysxManager::Lock()
	{
		Assert(scene);
		scene->lockWrite();
	}

	void PhysxManager::Unlock()
	{
		Assert(scene);
		scene->unlockWrite();
	}

	void PhysxManager::ReadLock()
	{
		Assert(scene);
		scene->lockRead();
	}

	void PhysxManager::ReadUnlock()
	{
		Assert(scene);
		scene->unlockRead();
	}

	ConvexHullSet *PhysxManager::GetCollisionMesh(shared_ptr<Model> model)
	{
		if (cache.count(model->name))
		{
			return cache[model->name];
		}

		return nullptr;
	}

	ConvexHullSet *PhysxManager::BuildConvexHulls(Model *model)
	{
		if (cache.count(model->name))
		{
			return cache[model->name];
		}

		Logf("Rebuilding convex hulls for %s", model->name);

		ConvexHullSet *set = new ConvexHullSet;
		ConvexHullBuilding::BuildConvexHulls(set, model);
		cache[model->name] = set;
		return set;
	}

	PxRigidActor *PhysxManager::CreateActor(shared_ptr<Model> model, ActorDesc desc)
	{
		Lock();
		PxRigidActor *actor;

		if (desc.dynamic)
			actor = physics->createRigidDynamic(desc.transform);
		else
			actor = physics->createRigidStatic(desc.transform);

		PxMaterial *mat = physics->createMaterial(0.6f, 0.5f, 0.0f);

		auto decomposition = BuildConvexHulls(model.get());

		for (auto hull : decomposition->hulls)
		{
			PxConvexMeshDesc convexDesc;
			convexDesc.points.count = hull.pointCount;
			convexDesc.points.stride = hull.pointByteStride;
			convexDesc.points.data = hull.points;
			convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

			PxDefaultMemoryOutputStream buf;
			PxConvexMeshCookingResult::Enum result;

			if (!pxCooking->cookConvexMesh(convexDesc, buf, &result))
			{
				Errorf("Failed to cook PhysX hull for %s", model->name);
				return nullptr;
			}

			PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
			PxConvexMesh *pxhull = physics->createConvexMesh(input);

			actor->createShape(PxConvexMeshGeometry(pxhull, desc.scale), *mat);
		}

		if (desc.dynamic)
			PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidDynamic *>(actor), 1.0f);

		scene->addActor(*actor);
		Unlock();
		return actor;
	}

	PxController *PhysxManager::CreateController(PxVec3 pos, float radius, float height, float density)
	{
		Lock();
		manager = PxCreateControllerManager(*scene, true);

		//Capsule controller description will want to be data driven
		PxCapsuleControllerDesc desc;
		desc.position = PxExtendedVec3(pos.x, pos.y, pos.z);
		desc.upDirection = PxVec3(0,1,0);
		desc.radius = radius;
		desc.height = height;
		desc.density = density;
		desc.material = physics->createMaterial(0.3f, 0.3f, 0.3f);

		PxController* controller = manager->createController(desc);
		Unlock();
		return controller;
	}
}
