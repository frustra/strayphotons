#include <PxScene.h>
#include "physx/PhysxManager.hh"

#include "assets/Model.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"

#include <VHACD.h>
#include <thread>
#include <unordered_map>

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

	class VHACDCallback : public VHACD::IVHACD::IUserCallback
	{
	public:
		VHACDCallback() {}
		~VHACDCallback() {}

		void Update(const double overallProgress, const double stageProgress, const double operationProgress,
					const char *const stage, const char *const operation)
		{
			//Logf("VHACD %d (%s)", (int)(overallProgress + 0.5), stage);
		};
	};

	struct ConvexHull
	{
		float *points;
		int nPoints;
	};

	struct ConvexHullSet
	{
		vector<ConvexHull> hulls;
	};

	ConvexHullSet *buildConvexHulls(Model *model, Model::Primitive *prim)
	{
		static std::unordered_map<std::string, ConvexHullSet *> cache;

		if (cache.count(model->name))
		{
			return cache[model->name];
		}

		Logf("Rebuilding convex decomposition for %s", model->name);

		auto posAttrib = prim->attributes[0];
		Assert(posAttrib.componentCount == 3);
		Assert(posAttrib.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

		auto indexAttrib = prim->indexBuffer;
		Assert(prim->drawMode == GL_TRIANGLES);
		Assert(indexAttrib.componentCount == 1);

		auto pbuffer = model->GetScene()->buffers[posAttrib.bufferName];
		auto points = (const float *)(pbuffer.data.data() + posAttrib.byteOffset);

		auto ibuffer = model->GetScene()->buffers[indexAttrib.bufferName];
		auto indices = (const int *)(ibuffer.data.data() + indexAttrib.byteOffset);
		int *indicesCopy = nullptr;

		switch (indexAttrib.componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				// indices is already compatible
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				indicesCopy = new int[indexAttrib.components];

				for (int i = 0; i < indexAttrib.components; i++)
				{
					indicesCopy[i] = (int)((const uint16 *) indices)[i];
				}

				indices = (const int *)indicesCopy;
				break;
			default:
				Assert(false, "invalid index component type");
				break;
		}

		static VHACDCallback vhacdCallback;
		auto decomposition = new ConvexHullSet;

		{
			auto interfaceVHACD = VHACD::CreateVHACD();
			int pointStride = posAttrib.byteStride / sizeof(float);

			VHACD::IVHACD::Parameters params;
			params.m_callback = &vhacdCallback;
			params.m_oclAcceleration = false;
			//params.m_resolution = 100000;
			//params.m_convexhullDownsampling = 8;

			bool res = interfaceVHACD->Compute(points, pointStride, posAttrib.components, indices, 3, indexAttrib.components / 3, params);
			Assert(res, "building convex decomposition");

			for (int i = 0; i < interfaceVHACD->GetNConvexHulls(); i++)
			{
				VHACD::IVHACD::ConvexHull ihull;
				interfaceVHACD->GetConvexHull(i, ihull);

				ConvexHull hull;
				hull.points = new float[ihull.m_nPoints * 3];
				hull.nPoints = ihull.m_nPoints;

				for (int i = 0; i < ihull.m_nPoints * 3; i++)
				{
					hull.points[i] = (float) ihull.m_points[i];
				}

				decomposition->hulls.push_back(hull);
			}

			interfaceVHACD->Clean();
			interfaceVHACD->Release();
		}

		cache[model->name] = decomposition;

		if (indicesCopy)
			delete indicesCopy;

		return decomposition;
	}

	PxRigidActor *PhysxManager::CreateActor(shared_ptr<Model> model, PxTransform transform, PxMeshScale scale, bool dynamic)
	{
		Lock();
		PxRigidActor *actor;

		if (dynamic)
			actor = physics->createRigidDynamic(transform);
		else
			actor = physics->createRigidStatic(transform);

		PxMaterial *mat = physics->createMaterial(0.6f, 0.5f, 0.0f);

		for (auto prim : model->primitives)
		{
			auto decomposition = buildConvexHulls(model.get(), prim);

			for (auto hull : decomposition->hulls)
			{
				PxConvexMeshDesc convexDesc;
				convexDesc.points.count = hull.nPoints;
				convexDesc.points.stride = sizeof(float) * 3;
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

				actor->createShape(PxConvexMeshGeometry(pxhull, scale), *mat);
			}
		};

		if (dynamic)
			PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidDynamic *>(actor), 1.0f);

		scene->addActor(*actor);
		Unlock();
		return actor;
	}
}
