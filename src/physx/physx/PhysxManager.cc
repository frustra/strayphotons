#include "PhysxManager.hh"

#include "PhysxUtils.hh"
#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "console/CFunc.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <PxScene.h>
#include <chrono>
#include <cmath>
#include <fstream>

namespace sp {
    using namespace physx;

    // clang-format off
    CVar<float> CVarGravity("x.Gravity", -9.81f, "Acceleration due to gravity (m/sec^2)");
    static CVar<bool> CVarPropJumping("x.PropJumping", false, "Disable player collision with held object");
    // clang-format on

    PhysxManager::PhysxManager()
        : RegisteredThread("PhysX", 120.0), characterControlSystem(*this), constraintSystem(*this),
          humanControlSystem(*this), physicsQuerySystem(*this) {
        Logf("PhysX %d.%d.%d starting up",
            PX_PHYSICS_VERSION_MAJOR,
            PX_PHYSICS_VERSION_MINOR,
            PX_PHYSICS_VERSION_BUGFIX);
        pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);

        PxTolerancesScale scale;

#ifndef SP_PACKAGE_RELEASE
        pxPvd = PxCreatePvd(*pxFoundation);
        pxPvdTransport = PxDefaultPvdSocketTransportCreate("localhost", 5425, 10);
        pxPvd->connect(*pxPvdTransport, PxPvdInstrumentationFlag::eALL);
        Logf("PhysX visual debugger listening on :5425");
#endif

        pxPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, scale, false, pxPvd);
        Assert(pxPhysics, "PxCreatePhysics");

        pxCooking = PxCreateCooking(PX_PHYSICS_VERSION, *pxFoundation, PxCookingParams(scale));
        Assert(pxCooking, "PxCreateCooking");

        scratchBlock.resize(0x1000000); // 16MiB

        CreatePhysxScene();
        StartThread();
    }

    PhysxManager::~PhysxManager() {
        StopThread();

        for (auto val : cache) {
            auto hulSet = val.second;
            for (auto hul : hulSet->hulls) {
                delete[] hul.points;
                delete[] hul.triangles;
            }
            delete val.second;
        }

        controllerManager.reset();
        controllerHitReporter.reset();
        scene.reset();
        if (dispatcher) {
            dispatcher->release();
            dispatcher = nullptr;
        }

        if (pxCooking) {
            pxCooking->release();
            pxCooking = nullptr;
        }
        if (pxPhysics) {
            pxPhysics->release();
            pxPhysics = nullptr;
        }
#ifndef SP_PACKAGE_RELEASE
        if (pxPvd) {
            pxPvd->release();
            pxPvd = nullptr;
        }
        if (pxPvdTransport) {
            pxPvdTransport->release();
            pxPvdTransport = nullptr;
        }
#endif
        if (pxFoundation) {
            pxFoundation->release();
            pxFoundation = nullptr;
        }
    }

    void PhysxManager::Frame() {
        // Wake up all actors and update the scene if gravity is changed.
        if (CVarGravity.Changed()) {
            scene->setGravity(PxVec3(0.f, CVarGravity.Get(true), 0.f));

            vector<PxActor *> buffer(256, nullptr);
            size_t startIndex = 0;

            while (true) {
                uint32_t n = scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, &buffer[0], buffer.size(), startIndex);

                for (uint32_t i = 0; i < n; i++) {
                    buffer[i]->is<PxRigidDynamic>()->wakeUp();
                }

                if (n < buffer.size()) break;

                startIndex += n;
            }
        }

        { // Sync ECS state to physx
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::Transform>,
                ecs::Write<ecs::Physics, ecs::HumanController, ecs::InteractController>>();

            // Delete actors for removed entities
            ecs::ComponentEvent<ecs::Physics> physicsEvent;
            while (physicsObserver.Poll(lock, physicsEvent)) {
                if (physicsEvent.type == Tecs::EventType::REMOVED && physicsEvent.component.actor) {
                    RemoveActor(physicsEvent.component.actor);
                }
            }
            ecs::ComponentEvent<ecs::HumanController> humanControllerEvent;
            while (humanControllerObserver.Poll(lock, humanControllerEvent)) {
                if (humanControllerEvent.type == Tecs::EventType::REMOVED &&
                    humanControllerEvent.component.pxController) {
                    RemoveController(humanControllerEvent.component.pxController);
                }
            }

            // Update controllers with latest entity data
            for (auto ent : lock.EntitiesWith<ecs::HumanController>()) {
                if (!ent.Has<ecs::HumanController, ecs::Transform>(lock)) continue;

                UpdateController(lock, ent);
            }

            // Update actors with latest entity data
            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::Transform>(lock)) continue;
                // Controllers take priority over actors
                if (ent.Has<ecs::HumanController>(lock)) continue;

                auto &ph = ent.Get<ecs::Physics>(lock);
                if (ph.model && !ph.model->Valid()) {
                    // Not all actors are ready, skip this frame.
                    return;
                }

                UpdateActor(lock, ent);
            }

            constraintSystem.Frame(lock);
        }

        humanControlSystem.Frame();
        characterControlSystem.Frame();
        physicsQuerySystem.Frame();

        { // Simulate 1 physics frame (blocking)
            scene->simulate(PxReal(std::chrono::nanoseconds(this->interval).count() / 1e9),
                nullptr,
                scratchBlock.data(),
                scratchBlock.size());
            scene->fetchResults(true);
        }

        { // Sync ECS state from physx
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Physics>, ecs::Write<ecs::Transform>>();

            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::Transform>(lock)) continue;

                auto &ph = ent.Get<ecs::Physics>(lock);
                auto &readTransform = ent.Get<const ecs::Transform>(lock);

                if (!ph.dynamic) continue;

                Assert(!readTransform.HasParent(lock), "Dynamic physics objects must have no parent");

                if (ph.actor) {
                    auto userData = (ActorUserData *)ph.actor->userData;
                    if (!readTransform.HasChanged(userData->transformChangeNumber)) {
                        auto &transform = ent.Get<ecs::Transform>(lock);
                        auto pose = ph.actor->getGlobalPose();
                        transform.SetPosition(PxVec3ToGlmVec3(pose.p));
                        transform.SetRotation(PxQuatToGlmQuat(pose.q));

                        userData->transformChangeNumber = transform.ChangeNumber();
                    }
                }
            }
        }

        triggerSystem.Frame();
    }

    void PhysxManager::CreatePhysxScene() {
        PxSceneDesc sceneDesc(pxPhysics->getTolerancesScale());

        sceneDesc.gravity = PxVec3(0.f, CVarGravity.Get(true), 0.f);
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;

        // Don't collide held model with player
        PxSetGroupCollisionFlag(PhysxCollisionGroup::HELD_OBJECT, PhysxCollisionGroup::PLAYER, false);
        // Don't collide anything with the noclip group.
        PxSetGroupCollisionFlag(PhysxCollisionGroup::WORLD, PhysxCollisionGroup::NOCLIP, false);
        PxSetGroupCollisionFlag(PhysxCollisionGroup::HELD_OBJECT, PhysxCollisionGroup::NOCLIP, false);

        dispatcher = PxDefaultCpuDispatcherCreate(1);
        sceneDesc.cpuDispatcher = dispatcher;

        auto pxScene = pxPhysics->createScene(sceneDesc);
        Assert(pxScene, "Failed to create PhysX scene");
        scene = std::shared_ptr<PxScene>(pxScene, [](PxScene *s) {
            s->release();
        });

        controllerHitReporter = make_unique<ControllerHitReport>();

        auto pxControllerManager = PxCreateControllerManager(*scene);
        controllerManager = std::shared_ptr<PxControllerManager>(pxControllerManager, [](PxControllerManager *m) {
            m->purgeControllers();
            m->release();
        });

        PxMaterial *groundMat = pxPhysics->createMaterial(0.6f, 0.5f, 0.0f);
        PxRigidStatic *groundPlane = PxCreatePlane(*pxPhysics, PxPlane(0.f, 1.f, 0.f, 1.03f), *groundMat);

        PxShape *shape;
        groundPlane->getShapes(&shape, 1);
        PxFilterData data;
        data.word0 = PhysxCollisionGroup::WORLD;
        shape->setQueryFilterData(data);
        shape->setSimulationFilterData(data);

        scene->addActor(*groundPlane);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            physicsObserver = lock.Watch<ecs::ComponentEvent<ecs::Physics>>();
            humanControllerObserver = lock.Watch<ecs::ComponentEvent<ecs::HumanController>>();
        }
    }

    ConvexHullSet *PhysxManager::GetCachedConvexHulls(std::string name) {
        if (cache.count(name)) { return cache[name]; }

        return nullptr;
    }

    ConvexHullSet *PhysxManager::BuildConvexHulls(const Model &model, bool decomposeHull) {
        ConvexHullSet *set;

        std::string name = model.name;
        if (decomposeHull) name += "-decompose";

        set = GetCachedConvexHulls(name);
        if (set) return set;

        set = LoadCollisionCache(model, decomposeHull);
        if (set) {
            cache[name] = set;
            return set;
        }

        Logf("Rebuilding convex hulls for %s", name);

        set = new ConvexHullSet;
        ConvexHullBuilding::BuildConvexHulls(set, model, decomposeHull);
        cache[name] = set;
        SaveCollisionCache(model, set, decomposeHull);
        return set;
    }

    void PhysxManager::Translate(PxRigidDynamic *actor, const PxVec3 &transform) {
        PxRigidBodyFlags flags = actor->getRigidBodyFlags();
        Assert(flags.isSet(PxRigidBodyFlag::eKINEMATIC), "cannot translate a non-kinematic actor");

        PxTransform pose = actor->getGlobalPose();
        pose.p += transform;
        actor->setKinematicTarget(pose);
    }

    void PhysxManager::UpdateActor(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::Physics>> lock,
        Tecs::Entity &e) {
        auto &ph = e.Get<ecs::Physics>(lock);
        auto &transform = e.Get<ecs::Transform>(lock);

        if (!ph.model || !ph.model->Valid()) return;

        auto globalTransform = transform.GetGlobalTransform(lock);
        auto scale = globalTransform.GetScale();

        auto pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform.GetPosition()),
            GlmQuatToPxQuat(globalTransform.GetRotation()));

        if (!ph.actor) {
            if (ph.dynamic) {
                Assert(!transform.HasParent(lock), "Dynamic physics objects must have no parent");

                ph.actor = pxPhysics->createRigidDynamic(pxTransform);

                if (ph.kinematic) { ph.actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true); }
            } else {
                ph.actor = pxPhysics->createRigidStatic(pxTransform);
            }
            Assert(ph.actor, "Physx did not return valid PxRigidActor");

            ph.actor->userData = new ActorUserData(e, transform.ChangeNumber());

            PxMaterial *mat = pxPhysics->createMaterial(0.6f, 0.5f, 0.0f);

            auto decomposition = BuildConvexHulls(*ph.model.get(), ph.decomposeHull);

            for (auto hull : decomposition->hulls) {
                PxConvexMeshDesc convexDesc;
                convexDesc.points.count = hull.pointCount;
                convexDesc.points.stride = hull.pointByteStride;
                convexDesc.points.data = hull.points;
                convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

                PxDefaultMemoryOutputStream buf;
                PxConvexMeshCookingResult::Enum result;

                if (!pxCooking->cookConvexMesh(convexDesc, buf, &result)) {
                    Errorf("Failed to cook PhysX hull for %s", ph.model->name);
                    return;
                }

                PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
                PxConvexMesh *pxhull = pxPhysics->createConvexMesh(input);

                auto shape = PxRigidActorExt::createExclusiveShape(*ph.actor,
                    PxConvexMeshGeometry(pxhull, PxMeshScale(GlmVec3ToPxVec3(scale))),
                    *mat);
                PxFilterData data;
                data.word0 = PhysxCollisionGroup::WORLD;
                shape->setQueryFilterData(data);
                shape->setSimulationFilterData(data);
            }

            if (ph.dynamic) { PxRigidBodyExt::updateMassAndInertia(*ph.actor->is<PxRigidDynamic>(), ph.density); }

            scene->addActor(*ph.actor);
        }

        auto userData = (ActorUserData *)ph.actor->userData;
        if (transform.HasChanged(userData->transformChangeNumber)) {
            if (ph.scale != scale) {
                auto n = ph.actor->getNbShapes();
                std::vector<PxShape *> shapes(n);
                ph.actor->getShapes(&shapes[0], n);
                for (uint32 i = 0; i < n; i++) {
                    PxConvexMeshGeometry geom;
                    if (shapes[i]->getConvexMeshGeometry(geom)) {
                        geom.scale = PxMeshScale(GlmVec3ToPxVec3(scale));
                        shapes[i]->setGeometry(geom);
                    } else {
                        Abort("Physx geometry type not implemented");
                    }
                }
                ph.scale = scale;
            }

            ph.actor->setGlobalPose(pxTransform);
            userData->transformChangeNumber = transform.ChangeNumber();
        }
    }

    void PhysxManager::RemoveActor(PxRigidActor *actor) {
        if (actor) {
            if (actor->userData) {
                delete (ActorUserData *)actor->userData;
                actor->userData = nullptr;
            }

            scene->removeActor(*actor);
            actor->release();
        }
    }

    void ControllerHitReport::onShapeHit(const PxControllerShapeHit &hit) {
        auto dynamic = hit.actor->is<PxRigidDynamic>();
        if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) {
            auto userData = (CharacterControllerUserData *)hit.controller->getUserData();
            auto magnitude = glm::length(userData->velocity);
            if (magnitude > 0.0001) {
                PxRigidBodyExt::addForceAtPos(*dynamic,
                    hit.dir.multiply(PxVec3(magnitude * ecs::PLAYER_PUSH_FORCE)),
                    PxVec3(hit.worldPos.x, hit.worldPos.y, hit.worldPos.z),
                    PxForceMode::eIMPULSE);
            }
        }
    }

    bool PhysxManager::MoveController(PxController *controller, double dt, PxVec3 displacement) {
        PxFilterData data;
        data.word0 = CVarPropJumping.Get() ? PhysxCollisionGroup::WORLD : PhysxCollisionGroup::PLAYER;
        PxControllerFilters filters(&data);
        auto flags = controller->move(displacement, 0, dt, filters);

        return flags & PxControllerCollisionFlag::eCOLLISION_DOWN;
    }

    void PhysxManager::UpdateController(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::HumanController>> lock,
        Tecs::Entity &e) {

        auto &controller = e.Get<ecs::HumanController>(lock);
        auto &transform = e.Get<ecs::Transform>(lock);

        auto position = transform.GetGlobalTransform(lock).GetPosition();
        // Offset the capsule position so the camera (transform origin) is at the top
        auto capsuleHeight = controller.pxController ? controller.pxController->getHeight() : controller.height;
        auto pxPosition = GlmVec3ToPxExtendedVec3(position - glm::vec3(0, capsuleHeight / 2, 0));

        if (!controller.pxController) {
            auto characterUserData = new CharacterControllerUserData(e, transform.ChangeNumber());

            PxCapsuleControllerDesc desc;
            desc.position = pxPosition;
            desc.upDirection = PxVec3(0, 1, 0);
            desc.radius = ecs::PLAYER_RADIUS;
            desc.height = controller.height;
            desc.density = 0.5f;
            desc.material = pxPhysics->createMaterial(0.3f, 0.3f, 0.3f);
            desc.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
            desc.reportCallback = controllerHitReporter.get();
            desc.userData = characterUserData;

            auto pxController = controllerManager->createController(desc);
            Assert(pxController->getType() == PxControllerShapeType::eCAPSULE,
                "Physx did not create a valid PxCapsuleController");

            pxController->setStepOffset(ecs::PLAYER_STEP_HEIGHT);
            auto actor = pxController->getActor();
            actor->userData = &characterUserData->actorData;

            PxShape *shape;
            actor->getShapes(&shape, 1);
            PxFilterData data;
            data.word0 = PhysxCollisionGroup::PLAYER;
            shape->setQueryFilterData(data);
            shape->setSimulationFilterData(data);

            controller.pxController = static_cast<PxCapsuleController *>(pxController);
        }

        auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
        if (transform.HasChanged(userData->actorData.transformChangeNumber)) {
            controller.pxController->setPosition(pxPosition);
            userData->actorData.transformChangeNumber = transform.ChangeNumber();
        }

        float currentHeight = controller.pxController->getHeight();
        if (currentHeight != controller.height) {
            auto currentPos = controller.pxController->getFootPosition();
            controller.pxController->resize(controller.height);
            if (!userData->onGround) {
                // If player is in the air, resize from the top to implement crouch-jumping.
                auto newPos = currentPos;
                newPos.y += currentHeight - controller.height;
                controller.pxController->setFootPosition(newPos);
            }

            physx::PxOverlapBuffer hit;
            physx::PxRigidDynamic *actor = controller.pxController->getActor();

            if (OverlapQuery(actor, physx::PxVec3(0), hit)) {
                // Revert to current height, since we collided with something
                controller.pxController->resize(currentHeight);
                controller.pxController->setFootPosition(currentPos);
            }
        }
    }

    void PhysxManager::RemoveController(physx::PxCapsuleController *controller) {
        if (controller) {
            auto userData = controller->getUserData();
            if (userData) {
                delete (CharacterControllerUserData *)userData;
                controller->setUserData(nullptr);
            }

            controller->release();
        }
    }

    bool PhysxManager::RaycastQuery(ecs::Lock<ecs::Read<ecs::HumanController>> lock,
        Tecs::Entity entity,
        glm::vec3 origin,
        glm::vec3 dir,
        const float distance,
        PxRaycastBuffer &hit) {
        PxRigidDynamic *controllerActor = nullptr;
        if (entity.Has<ecs::HumanController>(lock)) {
            auto &controller = entity.Get<ecs::HumanController>(lock);
            controllerActor = controller.pxController->getActor();
            scene->removeActor(*controllerActor);
        }

        bool status = scene->raycast(GlmVec3ToPxVec3(origin), GlmVec3ToPxVec3(dir), distance, hit);

        if (controllerActor) { scene->addActor(*controllerActor); }

        return status;
    }

    bool PhysxManager::SweepQuery(PxRigidDynamic *actor, PxVec3 dir, float distance, PxSweepBuffer &hit) {
        PxShape *shape;
        auto shapeCount = actor->getShapes(&shape, 1);
        Assert(shapeCount == 1, "PhysxManager::SweepQuery expected actor to have 1 shape");

        PxCapsuleGeometry capsuleGeometry;
        bool validCapsule = shape->getCapsuleGeometry(capsuleGeometry);
        Assert(validCapsule, "PhysxManager::SweepQuery expected actor to have capsule geometry");

        scene->removeActor(*actor);
        bool status = scene->sweep(capsuleGeometry, actor->getGlobalPose(), dir, distance, hit);
        scene->addActor(*actor);
        return status;
    }

    bool PhysxManager::OverlapQuery(PxRigidDynamic *actor, PxVec3 translation, PxOverlapBuffer &hit) {
        PxShape *shape;
        actor->getShapes(&shape, 1);

        PxCapsuleGeometry capsuleGeometry;
        shape->getCapsuleGeometry(capsuleGeometry);

        scene->removeActor(*actor);
        PxQueryFilterData filterData = PxQueryFilterData(
            PxQueryFlag::eANY_HIT | PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);
        PxTransform pose = actor->getGlobalPose();
        pose.p += translation;
        bool overlapFound = scene->overlap(capsuleGeometry, pose, hit, filterData);
        scene->addActor(*actor);

        return overlapFound;
    }

    void PhysxManager::EnableCollisions(PxRigidActor *actor, bool enabled) {
        PxU32 nShapes = actor->getNbShapes();
        vector<PxShape *> shapes(nShapes);
        actor->getShapes(shapes.data(), nShapes);

        for (uint32 i = 0; i < nShapes; ++i) {
            shapes[i]->setFlag(PxShapeFlag::eSIMULATION_SHAPE, enabled);
            shapes[i]->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, enabled);
        }
    }

    void PhysxManager::SetCollisionGroup(PxRigidActor *actor, PhysxCollisionGroup group) {
        PxU32 nShapes = actor->getNbShapes();
        vector<PxShape *> shapes(nShapes);
        actor->getShapes(shapes.data(), nShapes);

        PxFilterData data;
        data.word0 = group;
        for (uint32 i = 0; i < nShapes; ++i) {
            shapes[i]->setQueryFilterData(data);
            shapes[i]->setSimulationFilterData(data);
        }
    }

    // Increment if the Collision Cache format ever changes
    const uint32 hullCacheMagic = 0xc042;

    ConvexHullSet *PhysxManager::LoadCollisionCache(const Model &model, bool decomposeHull) {
        std::ifstream in;

        std::string path = "cache/collision/" + model.name;
        if (decomposeHull) path += "-decompose";

        if (GAssets.InputStream(path, AssetType::Bundled, in)) {
            uint32 magic;
            in.read((char *)&magic, 4);
            if (magic != hullCacheMagic) {
                Logf("Ignoring outdated collision cache format for %s", path);
                in.close();
                return nullptr;
            }

            int32 bufferCount;
            in.read((char *)&bufferCount, 4);
            Assert(bufferCount > 0, "hull cache buffer count");

            char bufferName[256] = {'\0'};

            for (int i = 0; i < bufferCount; i++) {
                uint32 nameLen;
                in.read((char *)&nameLen, 4);
                Assert(nameLen <= 256, "hull cache buffer name too long on read");

                in.read(bufferName, nameLen);
                string name(bufferName, nameLen);

                Hash128 hash;
                in.read((char *)hash.data(), sizeof(hash));

                int bufferIndex = std::stoi(name);

                if (!model.HasBuffer(bufferIndex) || model.HashBuffer(bufferIndex) != hash) {
                    Logf("Ignoring outdated collision cache for %s", name);
                    in.close();
                    return nullptr;
                }
            }

            int32 hullCount;
            in.read((char *)&hullCount, 4);
            Assert(hullCount > 0, "hull cache count");

            ConvexHullSet *set = new ConvexHullSet;
            set->hulls.reserve(hullCount);

            for (int i = 0; i < hullCount; i++) {
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

    void PhysxManager::SaveCollisionCache(const Model &model, ConvexHullSet *set, bool decomposeHull) {
        std::ofstream out;
        std::string name = "cache/collision/" + model.name;
        if (decomposeHull) name += "-decompose";

        if (GAssets.OutputStream(name, out)) {
            out.write((char *)&hullCacheMagic, 4);

            int32 bufferCount = set->bufferIndexes.size();
            out.write((char *)&bufferCount, 4);

            for (int bufferIndex : set->bufferIndexes) {
                Hash128 hash = model.HashBuffer(bufferIndex);
                string bufferName = std::to_string(bufferIndex);
                uint32 nameLen = bufferName.length();
                Assert(nameLen <= 256, "hull cache buffer name too long on write");

                out.write((char *)&nameLen, 4);
                out.write(bufferName.c_str(), nameLen);
                out.write((char *)hash.data(), sizeof(hash));
            }

            int32 hullCount = set->hulls.size();
            out.write((char *)&hullCount, 4);

            for (auto hull : set->hulls) {
                out.write((char *)&hull, 4 * sizeof(uint32));
                out.write((char *)hull.points, hull.pointCount * hull.pointByteStride);
                out.write((char *)hull.triangles, hull.triangleCount * hull.triangleByteStride);
            }

            out.close();
        }
    }
} // namespace sp
