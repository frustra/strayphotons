#include <PxScene.h>

#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include "ecs/components/Controller.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Physics.hh"

#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"

#include <fstream>

namespace sp
{
	using namespace physx;
	static CVar<float> CVarGravity("g.Gravity", -9.81f, "Acceleration due to gravity (m/sec^2)");

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
		Lock();
		exiting = true;
		thread.join();

		for (auto val : cache)
		{
			auto hulSet = val.second;
			for (auto hul : hulSet->hulls)
			{
				delete[] hul.points;
				delete[] hul.triangles;
			}
			delete val.second;
		}

		if (manager)
		{
			manager->purgeControllers();
			manager->release();
		}
		Unlock();
		DestroyPhysxScene();

		pxCooking->release();
		physics->release();
		pxFoundation->release();
	}

	void PhysxManager::Frame(double timeStep)
	{
		bool hadResults = false;

		for (PhysxConstraint& constraint : constraints)
		{
			auto transform = constraint.parent.Get<ecs::Transform>();
			PxTransform destination;

			if(constraint.parent.Has<ecs::Physics>())
			{
				auto physics = constraint.parent.Get<ecs::Physics>();
				destination = physics->actor->getGlobalPose();

			}
			else if (constraint.parent.Has<ecs::HumanController>())
			{
				auto controller = constraint.parent.Get<ecs::HumanController>();
				PxController* pxController = controller->pxController;
				destination = pxController->getActor()->getGlobalPose();
			}
			else
			{
				std::cout << "Error physics constraint, parent not registered with physics system!";
				continue;
			}
			glm::vec3 forward = glm::vec3(0,0,-1);
			glm::vec3 rotate = forward * transform->rotate;
			PxVec3 dir = GlmVec3ToPxVec3(rotate);
			dir.normalizeSafe();

			destination.p += (dir * 3.f);
			constraint.child->setKinematicTarget(destination);
		}

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
		thread = std::thread([&]
		{
			const double rate = 60.0; // frames/sec

			while (!exiting)
			{
				double frameStart = glfwGetTime();

				if (simulate)
					Frame(1.0 / rate);

				double frameEnd = glfwGetTime();
				double sleepFor = 1.0 / rate + frameStart - frameEnd;

				std::this_thread::sleep_for(std::chrono::nanoseconds((uint64) (sleepFor * 1e9)));
			}
		});
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

	void PhysxManager::Lock()
	{
		Assert(scene, "physx scene is null");
		scene->lockWrite();
	}

	void PhysxManager::Unlock()
	{
		Assert(scene, "physx scene is null");
		scene->unlockWrite();
	}

	void PhysxManager::ReadLock()
	{
		Assert(scene, "physx scene is null");
		scene->lockRead();
	}

	void PhysxManager::ReadUnlock()
	{
		Assert(scene, "physx scene is null");
		scene->unlockRead();
	}

	ConvexHullSet *PhysxManager::GetCachedConvexHulls(Model *model)
	{
		if (cache.count(model->name))
		{
			return cache[model->name];
		}

		return nullptr;
	}

	ConvexHullSet *PhysxManager::BuildConvexHulls(Model *model)
	{
		ConvexHullSet *set;

		if ((set = GetCachedConvexHulls(model)))
			return set;

		if ((set = LoadCollisionCache(model)))
		{
			cache[model->name] = set;
			return set;
		}

		Logf("Rebuilding convex hulls for %s", model->name);

		set = new ConvexHullSet;
		ConvexHullBuilding::BuildConvexHulls(set, model);
		cache[model->name] = set;
		SaveCollisionCache(model, set);
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

	void PhysxManager::RemoveActor(PxRigidActor *actor)
	{
		Lock();
		// TODO(xthexder): Check this for memory leaks
		scene->removeActor(*actor);
		actor->release();
		Unlock();
	}

	PxController *PhysxManager::CreateController(PxVec3 pos, float radius, float height, float density)
	{
		Lock();
		if (!manager) manager = PxCreateControllerManager(*scene, true);

		//Capsule controller description will want to be data driven
		PxCapsuleControllerDesc desc;
		desc.position = PxExtendedVec3(pos.x, pos.y, pos.z);
		desc.upDirection = PxVec3(0, 1, 0);
		desc.radius = radius;
		desc.height = height;
		desc.density = density;
		desc.material = physics->createMaterial(0.3f, 0.3f, 0.3f);

		PxController *controller = manager->createController(desc);
		Unlock();
		return controller;
	}

	void PhysxManager::RemoveController(PxController *controller)
	{
		Lock();
		controller->release();
		Unlock();
	}

	bool PhysxManager::RaycastQuery(ecs::Entity& entity, const PxVec3 origin, const PxVec3 dir, const float distance, PxRaycastBuffer& hit)
	{
		Lock();
		scene->lockRead();

		physx::PxRigidDynamic* controllerActor  = nullptr;
		if (entity.Has<ecs::HumanController>())
		{
			auto controller = entity.Get<ecs::HumanController>();
			controllerActor = controller->pxController->getActor();
			scene->removeActor(*controllerActor);
		}

		bool status = scene->raycast(origin, dir, distance, hit);

		if (controllerActor)
		{
			scene->addActor(*controllerActor);
		}

		scene->unlockRead();
		Unlock();

		return status;
	}

	void PhysxManager::CreateConstraint(ecs::Entity parent, PxRigidDynamic* child, PxVec3 offset)
	{
		PhysxConstraint constraint;
		constraint.parent = parent;
		constraint.child = child;
		constraint.offset = offset;

		if(parent.Has<ecs::Physics>() || parent.Has<ecs::HumanController>())
		{
			constraints.emplace_back(constraint);
		}
	}

	const uint32 hullCacheMagic = 0xc040;

	ConvexHullSet *PhysxManager::LoadCollisionCache(Model *model)
	{
		std::ifstream in;

		if (GAssets.InputStream("cache/collision/" + model->name, in))
		{
			uint32 magic;
			in.read((char *)&magic, 4);
			Assert(magic == hullCacheMagic, "hull cache magic");

			int32 hullCount;
			in.read((char *)&hullCount, 4);
			Assert(hullCount > 0, "hull cache count");

			ConvexHullSet *set = new ConvexHullSet;
			set->hulls.reserve(hullCount);

			for (int i = 0; i < hullCount; i++)
			{
				ConvexHull hull;
				in.read((char *)&hull, 4 * sizeof(uint32));

				Assert(hull.pointByteStride % sizeof(float) == 0, "convex hull byte stride is odd");
				Assert(hull.triangleByteStride % sizeof(int) == 0, "convex hull byte stride is odd");

				hull.points = new float[hull.pointCount * hull.pointByteStride / sizeof(float)];
				hull.triangles = new int[hull.triangleCount * hull.triangleByteStride / sizeof(int)];

				in.read((char *)hull.points, hull.pointCount * hull.pointByteStride);
				in.read((char *)hull.triangles, hull.triangleCount * hull.triangleByteStride);

				set->hulls.push_back(hull);
			}

			in.close();
			return set;
		}

		return nullptr;
	}

	void PhysxManager::SaveCollisionCache(Model *model, ConvexHullSet *set)
	{
		std::ofstream out;

		if (GAssets.OutputStream("cache/collision/" + model->name, out))
		{
			out.write((char *)&hullCacheMagic, 4);

			int32 hullCount = set->hulls.size();
			out.write((char *)&hullCount, 4);

			for (auto hull : set->hulls)
			{
				out.write((char *)&hull, 4 * sizeof(uint32));
				out.write((char *)hull.points, hull.pointCount * hull.pointByteStride);
				out.write((char *)hull.triangles, hull.triangleCount * hull.triangleByteStride);
			}

			out.close();
		}
	}
}
