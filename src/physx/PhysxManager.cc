#include <PxScene.h>

#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include "ecs/components/Controller.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Interact.hh"

#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "core/CFunc.hh"

#include <fstream>

namespace sp
{
	using namespace physx;
	static CVar<float> CVarGravity("x.Gravity", -9.81f, "Acceleration due to gravity (m/sec^2)");
	static CVar<bool> CVarShowShapes("x.ShowShapes", false, "Show (1) or hide (0) the outline of physx collision shapes");

	PhysxManager::PhysxManager()
		:
		funcs(this)
	{
		funcs.Register("p.connectPVD", "Connect to a running PVD", &PhysxManager::ConnectToPVD);

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

		for (auto constraint = constraints.begin(); constraint != constraints.end();)
		{
			auto transform = constraint->parent.Get<ecs::Transform>();
			PxTransform destination;

			if (constraint->parent.Has<ecs::Physics>())
			{
				auto physics = constraint->parent.Get<ecs::Physics>();
				destination = physics->actor->getGlobalPose();
			}
			else if (constraint->parent.Has<ecs::HumanController>())
			{
				auto controller = constraint->parent.Get<ecs::HumanController>();
				PxController *pxController = controller->pxController;
				destination = pxController->getActor()->getGlobalPose();
			}
			else
			{
				std::cout << "Error physics constraint, parent not registered with physics system!";
				continue;
			}
			glm::vec3 rotate = transform->GetRotate() * PxVec3ToGlmVec3P(constraint->offset);
			PxVec3 targetPos = GlmVec3ToPxVec3(transform->GetPosition() + rotate);
			auto currentPos = constraint->child->getGlobalPose().transform(constraint->child->getCMassLocalPose().transform(physx::PxVec3(0.0)));
			auto distance = targetPos - currentPos;

			if (distance.magnitude() < 2.0)
			{
				constraint->child->setAngularVelocity(constraint->rotation);
				constraint->rotation = PxVec3(0); // Don't continue to rotate
				constraint->child->setLinearVelocity(distance.multiply(PxVec3(20.0)));
				constraint++;
			}
			else
			{
				// Remove the constraint if the distance is too far
				if (constraint->parent.Has<ecs::InteractController>())
				{
					constraint->parent.Get<ecs::InteractController>()->target = nullptr;
				}
				constraint = constraints.erase(constraint);
			}
		}

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

		if (CVarShowShapes.Changed())
		{
			ToggleDebug(CVarShowShapes.Get(true));
		}
		if (CVarShowShapes.Get())
		{
			CacheDebugLines();
		}

		scene->simulate((PxReal) timeStep);
		resultsPending = true;
		Unlock();
	}

	bool PhysxManager::LogicFrame(ecs::EntityManager &manager)
	{
		{
			// Sync transforms to physx
			bool gotLock = false;

			for (ecs::Entity ent : manager.EntitiesWith<ecs::Physics, ecs::Transform>())
			{
				auto ph = ent.Get<ecs::Physics>();
				auto transform = ent.Get<ecs::Transform>();

				if (ph->actor && transform->ClearDirty())
				{
					if (!gotLock)
					{
						Lock();
						gotLock = true;
					}

					auto position = transform->GetGlobalTransform() * glm::vec4(0, 0, 0, 1);
					auto rotate = transform->GetGlobalRotation();

					auto lastScale = ph->scale;
					auto newScale = glm::vec3(transform->GetScale() * glm::vec4(1, 1, 1, 0));
					if (lastScale != newScale)
					{
						auto n = ph->actor->getNbShapes();
						vector<physx::PxShape *> shapes(n);
						ph->actor->getShapes(&shapes[0], n);
						for (uint32 i = 0; i < n; i++)
						{
							physx::PxConvexMeshGeometry geom;
							if (shapes[i]->getConvexMeshGeometry(geom))
							{
								geom.scale = physx::PxMeshScale(GlmVec3ToPxVec3(newScale), physx::PxQuat(physx::PxIdentity));
								shapes[i]->setGeometry(geom);
							}
							else Assert(false, "Physx geometry type not implemented");
						}
					}


					physx::PxTransform newPose(GlmVec3ToPxVec3(position), GlmQuatToPxQuat(rotate));
					ph->actor->setGlobalPose(newPose);
				}
			}

			if (gotLock)
				Unlock();
		}

		{
			// Sync transforms from physx
			ReadLock();

			for (ecs::Entity ent : manager.EntitiesWith<ecs::Physics, ecs::Transform>())
			{
				auto ph = ent.Get<ecs::Physics>();
				auto transform = ent.Get<ecs::Transform>();

				Assert(!transform->HasParent(), "Physics objects must have no parent");

				if (ph->actor)
				{
					auto pose = ph->actor->getGlobalPose();
					transform->SetPosition(PxVec3ToGlmVec3P(pose.p));
					transform->SetRotate(PxQuatToGlmQuat(pose.q));
					transform->ClearDirty();
				}
			}

			ReadUnlock();
		}
		return true;
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

	void PhysxManager::ToggleDebug(bool enabled)
	{
		debug = enabled;
		float scale = (enabled ? 1.0f : 0.0f);

		Lock();
		scene->setVisualizationParameter(
			physx::PxVisualizationParameter::eSCALE, scale);

		scene->setVisualizationParameter(
			PxVisualizationParameter::eCOLLISION_SHAPES, scale);
		Unlock();
	}

	bool PhysxManager::IsDebugEnabled() const
	{
		return debug;
	}

	void PhysxManager::CacheDebugLines()
	{
		const physx::PxRenderBuffer &rb = scene->getRenderBuffer();
		const physx::PxDebugLine *lines = rb.getLines();

		{
			std::lock_guard<std::mutex> lock(debugLinesMutex);
			debugLines = vector<physx::PxDebugLine>(lines, lines + rb.getNbLines());
		}
	}

	MutexedVector<physx::PxDebugLine> PhysxManager::GetDebugLines()
	{
		return MutexedVector<physx::PxDebugLine>(debugLines, debugLinesMutex);
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

	void PhysxManager::Translate(
		physx::PxRigidDynamic *actor,
		const physx::PxVec3 &transform)
	{
		Lock();

		physx::PxRigidBodyFlags flags = actor->getRigidBodyFlags();
		if (!flags.isSet(physx::PxRigidBodyFlag::eKINEMATIC))
		{
			throw std::runtime_error("cannot translate a non-kinematic actor");
		}

		physx::PxTransform pose = actor->getGlobalPose();
		pose.p += transform;
		actor->setKinematicTarget(pose);
		Unlock();
	}

	void PhysxManager::DisableCollisions(physx::PxRigidActor *actor)
	{
		ToggleCollisions(actor, false);
	}

	void PhysxManager::EnableCollisions(physx::PxRigidActor *actor)
	{
		ToggleCollisions(actor, true);
	}

	void PhysxManager::ToggleCollisions(physx::PxRigidActor *actor, bool enabled)
	{
		Lock();

		physx::PxU32 nShapes = actor->getNbShapes();
		vector<physx::PxShape *> shapes(nShapes);
		actor->getShapes(shapes.data(), nShapes);

		for (uint32 i = 0; i < nShapes; ++i)
		{
			shapes[i]->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, enabled);
			shapes[i]->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, enabled);
		}

		Unlock();
	}

	PxRigidActor *PhysxManager::CreateActor(shared_ptr<Model> model, ActorDesc desc, const ecs::Entity &entity)
	{
		Lock();
		PxRigidActor *actor;

		if (desc.dynamic)
		{
			actor = physics->createRigidDynamic(desc.transform);

			if (desc.kinematic)
			{
				auto rigidBody = static_cast<physx::PxRigidBody *>(actor);
				rigidBody->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
			}
		}
		else
		{
			actor = physics->createRigidStatic(desc.transform);
		}

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

		static_assert(sizeof(void *) == sizeof(ecs::id_t),
			"wrong size of Entity::Id; it must be same size as a pointer");
		actor->userData = reinterpret_cast<void *>(entity.GetId().GetId());

		scene->addActor(*actor);
		Unlock();
		return actor;
	}

	void PhysxManager::RemoveActor(PxRigidActor *actor)
	{
		Lock();
		scene->removeActor(*actor);
		actor->release();
		Unlock();
	}

	ecs::Entity::Id PhysxManager::GetEntityId(const physx::PxActor &actor) const
	{
		static_assert(sizeof(ecs::id_t) == sizeof(physx::PxActor::userData),
			"size mismatch");
		return ecs::Entity::Id(reinterpret_cast<ecs::id_t>(actor.userData));
	}

	void ControllerHitReport::onShapeHit(const physx::PxControllerShapeHit &hit)
	{
		auto dynamic = hit.actor->isRigidDynamic();
		if (dynamic && !dynamic->getRigidDynamicFlags().isSet(PxRigidDynamicFlag::eKINEMATIC))
		{
			manager->Lock();
			glm::vec3 *velocity = (glm::vec3 *) hit.controller->getUserData();
			PxRigidBodyExt::addForceAtPos(
				*dynamic,
				hit.dir.multiply(PxVec3(glm::length(*velocity) * ecs::PLAYER_PUSH_FORCE)),
				PxVec3(hit.worldPos.x, hit.worldPos.y, hit.worldPos.z),
				PxForceMode::eIMPULSE
			);
			manager->Unlock();
		}
	}

	PxCapsuleController *PhysxManager::CreateController(PxVec3 pos, float radius, float height, float density)
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
		desc.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
		desc.reportCallback = new ControllerHitReport(this);
		desc.userData = new glm::vec3(0);

		PxCapsuleController *controller =
			static_cast<PxCapsuleController *>(manager->createController(desc));
		Unlock();
		return controller;
	}

	void PhysxManager::MoveController(PxController *controller, double dt, physx::PxVec3 displacement)
	{
		Lock();
		physx::PxControllerFilters filters;
		controller->move(displacement, 0, dt, filters);
		Unlock();
	}

	void PhysxManager::TeleportController(physx::PxController *controller, physx::PxExtendedVec3 position)
	{
		Lock();
		controller->setPosition(position);
		Unlock();
	}

	void PhysxManager::SetControllerHeight(
		PxCapsuleController *controller,
		const float capsuleHeight)
	{
		Lock();
		controller->setHeight(capsuleHeight);
		Unlock();
	}

	void PhysxManager::RemoveController(PxController *controller)
	{
		Lock();
		controller->release();
		Unlock();
	}

	float PhysxManager::GetCapsuleHeight(PxCapsuleController *controller)
	{
		ReadLock();
		float height = controller->getHeight();
		ReadUnlock();
		return height;
	}

	bool PhysxManager::RaycastQuery(ecs::Entity &entity, const PxVec3 origin, const PxVec3 dir, const float distance, PxRaycastBuffer &hit)
	{
		Lock();
		scene->lockRead();

		physx::PxRigidDynamic *controllerActor  = nullptr;
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

	bool PhysxManager::SweepQuery(PxRigidDynamic *actor, const PxVec3 dir, const float distance)
	{
		Lock();
		PxShape *shape;
		actor->getShapes(&shape, 1);

		PxCapsuleGeometry capsuleGeometry;
		shape->getCapsuleGeometry(capsuleGeometry);

		scene->removeActor(*actor);
		PxSweepBuffer hit;
		bool status = scene->sweep(capsuleGeometry, actor->getGlobalPose(), dir, distance, hit);
		scene->addActor(*actor);
		Unlock();
		return status;
	}

	bool PhysxManager::OverlapQuery(PxRigidDynamic *actor, PxVec3 translation, PxOverlapBuffer& hit)
	{
		Lock();
		PxShape *shape;
		actor->getShapes(&shape, 1);

		PxCapsuleGeometry capsuleGeometry;
		shape->getCapsuleGeometry(capsuleGeometry);

		scene->removeActor(*actor);
		physx::PxQueryFilterData filterData = physx::PxQueryFilterData(physx::PxQueryFlag::eANY_HIT | PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);
		physx::PxTransform pose = actor->getGlobalPose();
		pose.p += translation;
		bool overlapFound = scene->overlap(capsuleGeometry, pose, hit, filterData);
		scene->addActor(*actor);
		Unlock();
		return overlapFound;
	}

	void PhysxManager::CreateConstraint(ecs::Entity parent, PxRigidDynamic *child, PxVec3 offset)
	{
		Lock();
		PhysxConstraint constraint;
		constraint.parent = parent;
		constraint.child = child;
		constraint.offset = offset;
		constraint.rotation = PxVec3(0);

		if (parent.Has<ecs::Physics>() || parent.Has<ecs::HumanController>())
		{
			constraints.emplace_back(constraint);
		}
		Unlock();
	}

	void PhysxManager::RotateConstraint(ecs::Entity parent, PxRigidDynamic *child, PxVec3 rotation)
	{
		Lock();
		for (auto it = constraints.begin(); it != constraints.end();)
		{
			if (it->parent == parent && it->child == child)
			{
				it->rotation = rotation;
				break;
			}
		}
		Unlock();
	}

	void PhysxManager::RemoveConstraints(ecs::Entity parent, physx::PxRigidDynamic *child)
	{
		Lock();
		for (auto it = constraints.begin(); it != constraints.end();)
		{
			if (it->parent == parent && it->child == child)
			{
				it = constraints.erase(it);
			}
			else it++;
		}
		Unlock();
	}

	const uint32 hullCacheMagic = 0xc041;

	ConvexHullSet *PhysxManager::LoadCollisionCache(Model *model)
	{
		std::ifstream in;

		if (GAssets.InputStream("cache/collision/" + model->name, in))
		{
			uint32 magic;
			in.read((char *)&magic, 4);
			if (magic != hullCacheMagic)
			{
				Logf("Ignoring outdated collision cache format for %s", model->name);
				in.close();
				return nullptr;
			}

			int32 bufferCount;
			in.read((char *)&bufferCount, 4);
			Assert(bufferCount > 0, "hull cache buffer count");

			char bufferName[256];

			for (int i = 0; i < bufferCount; i++)
			{
				uint32 nameLen;
				in.read((char *)&nameLen, 4);
				Assert(nameLen <= 256, "hull cache buffer name too long on read");

				in.read(bufferName, nameLen);
				string name(bufferName, nameLen);

				Hash128 hash;
				in.read((char *)hash.data(), sizeof(hash));

				if (!model->HasBuffer(name) || model->HashBuffer(name) != hash)
				{
					Logf("Ignoring outdated collision cache for %s", model->name);
					in.close();
					return nullptr;
				}
			}

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

			int32 bufferCount = set->bufferNames.size();
			out.write((char *)&bufferCount, 4);

			for (auto bufferName : set->bufferNames)
			{
				Hash128 hash = model->HashBuffer(bufferName);
				uint32 nameLen = bufferName.length();
				Assert(nameLen <= 256, "hull cache buffer name too long on write");

				out.write((char *)&nameLen, 4);
				out.write(bufferName.c_str(), nameLen);
				out.write((char *)hash.data(), sizeof(hash));
			}

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

	void PhysxManager::ConnectToPVD(const string &)
	{
#if (defined(_WIN32) || defined(__APPLE__)) && !defined(PACKAGE_RELEASE)
		if (!physics->getPvdConnectionManager())
		{
		    std::cout << "Run PhysX Visual Debugger and connect to 127.0.0.1:5425\n";
		    return;
		}

		const char* ip = "127.0.0.1";
		int port = 5425;
		unsigned int timeout = 100;
		PxVisualDebuggerConnectionFlags flags =
		      PxVisualDebuggerConnectionFlag::eDEBUG
		    | PxVisualDebuggerConnectionFlag::ePROFILE
		    | PxVisualDebuggerConnectionFlag::eMEMORY;

		debugger::comm::PvdConnection* conn = PxVisualDebuggerExt::createConnection(physics->getPvdConnectionManager(), ip, port, timeout, flags);

		if (conn)
		{
		    std::cout << "Connected to PVD!\n";
		}
#elif
		std::cout << "PhysX Visual Debugger not supported for this platform or build\n";
#endif
	}
}
