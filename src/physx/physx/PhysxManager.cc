#include "PhysxManager.hh"

#include "PhysxUtils.hh"
#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"

#include <PxScene.h>
#include <chrono>
#include <cmath>
#include <fstream>

namespace sp {
    using namespace physx;

    // clang-format off
    CVar<float> CVarGravity("x.Gravity", -9.81f, "Acceleration due to gravity (m/sec^2)");
    // clang-format on

    PhysxManager::PhysxManager(bool stepMode)
        : RegisteredThread("PhysX", 120.0, true), scenes(GetSceneManager()), stepMode(stepMode),
          characterControlSystem(*this), constraintSystem(*this), physicsQuerySystem(*this), laserSystem(*this),
          animationSystem(*this), workQueue("PhysXHullLoading") {
        Logf("PhysX %d.%d.%d starting up",
            PX_PHYSICS_VERSION_MAJOR,
            PX_PHYSICS_VERSION_MINOR,
            PX_PHYSICS_VERSION_BUGFIX);
        pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);

#ifndef SP_PACKAGE_RELEASE
        pxPvd = PxCreatePvd(*pxFoundation);
        pxPvdTransport = PxDefaultPvdSocketTransportCreate("localhost", 5425, 10);
        if (pxPvd->connect(*pxPvdTransport, PxPvdInstrumentationFlag::eALL)) {
            Logf("PhysX visual debugger connected on :5425");
        } else {
            Logf("Could not connect to PhysX visual debugger on :5425");
        }
#endif

        PxTolerancesScale scale;
        pxPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, scale, false, pxPvd);
        Assert(pxPhysics, "PxCreatePhysics");
        Assert(PxInitExtensions(*pxPhysics, pxPvd), "PxInitExtensions");

        pxCooking = PxCreateCooking(PX_PHYSICS_VERSION, *pxFoundation, PxCookingParams(scale));
        Assert(pxCooking, "PxCreateCooking");

        scratchBlock.resize(0x1000000); // 16MiB

        CreatePhysxScene();
        if (stepMode) {
            funcs.Register<int>("stepphysics",
                "Advance the physics simulation by N frames, default is 1",
                [this](int arg) {
                    maxStepCount += std::max(1, arg);
                    auto step = stepCount.load();
                    while (step < maxStepCount) {
                        stepCount.wait(step);
                        step = stepCount.load();
                    }
                });
        }
        StartThread();
    }

    PhysxManager::~PhysxManager() {
        StopThread();

        controllerManager.reset();
        for (auto &joint : joints) {
            joint.second->release();
        }
        joints.clear();
        for (auto &actor : actors) {
            RemoveActor(actor.second);
        }
        actors.clear();
        scene.reset();
        cache.DropAll();

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
        PxCloseExtensions();
        if (pxFoundation) {
            pxFoundation->release();
            pxFoundation = nullptr;
        }
    }

    void PhysxManager::Frame() {
        scenes.PreloadScenePhysics([this](auto lock, auto scene) {
            bool complete = true;
            for (auto ent : lock.template EntitiesWith<ecs::Physics>()) {
                if (!ent.template Has<ecs::SceneInfo, ecs::Physics>(lock)) continue;
                if (ent.template Get<ecs::SceneInfo>(lock).scene.lock() != scene) continue;

                auto &ph = ent.template Get<ecs::Physics>(lock);
                if (ph.model && ph.model->Valid()) {
                    LoadConvexHullSet(*ph.model, ph.decomposeHull);
                } else {
                    complete = false;
                }
            }
            return complete;
        });

        if (stepMode && stepCount >= maxStepCount) return;

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

        animationSystem.Frame();

        { // Sync ECS state to physx
            ZoneScopedN("Sync from ECS");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
                                                        ecs::SignalOutput,
                                                        ecs::SignalBindings,
                                                        ecs::FocusLayer,
                                                        ecs::FocusLock,
                                                        ecs::Physics>,
                ecs::Write<ecs::Animation, ecs::TransformTree, ecs::CharacterController, ecs::EventInput>>();

            // Delete actors for removed entities
            ecs::ComponentEvent<ecs::Physics> physicsEvent;
            while (physicsObserver.Poll(lock, physicsEvent)) {
                if (physicsEvent.type == Tecs::EventType::REMOVED) {
                    if (actors.count(physicsEvent.entity) > 0) {
                        RemoveActor(actors[physicsEvent.entity]);
                        actors.erase(physicsEvent.entity);
                    }
                }
            }

            // Update actors with latest entity data
            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
                UpdateActor(lock, ent);
            }

            characterControlSystem.Frame(lock);
            constraintSystem.Frame(lock);
        }

        { // Simulate 1 physics frame (blocking)
            ZoneScopedN("Simulate");
            scene->simulate(PxReal(std::chrono::nanoseconds(this->interval).count() / 1e9),
                nullptr,
                scratchBlock.data(),
                scratchBlock.size());
            scene->fetchResults(true);
        }

        { // Sync ECS state from physx
            ZoneScopedN("Sync to ECS");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
                                                        ecs::SignalOutput,
                                                        ecs::SignalBindings,
                                                        ecs::FocusLayer,
                                                        ecs::FocusLock,
                                                        ecs::LaserEmitter,
                                                        ecs::Mirror>,
                ecs::Write<ecs::Animation,
                    ecs::TransformSnapshot,
                    ecs::TransformTree,
                    ecs::Physics,
                    ecs::PhysicsQuery,
                    ecs::LaserLine,
                    ecs::LaserSensor,
                    ecs::SignalOutput>>();

            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::TransformSnapshot, ecs::TransformTree>(lock)) continue;

                auto &ph = ent.Get<ecs::Physics>(lock);
                if (actors.count(ent) > 0) {
                    auto const &actor = actors[ent];
                    auto &transform = ent.Get<ecs::TransformSnapshot>(lock);

                    auto userData = (ActorUserData *)actor->userData;
                    Assert(userData, "Physics actor is missing UserData");
                    if (ph.dynamic && !ph.kinematic && transform.matrix == userData->pose.matrix) {
                        auto pose = actor->getGlobalPose();
                        transform.SetPosition(PxVec3ToGlmVec3(pose.p));
                        transform.SetRotation(PxQuatToGlmQuat(pose.q));
                        ent.Set<ecs::TransformTree>(lock, transform);
                        userData->velocity = transform.GetPosition() - userData->pose.GetPosition();
                    } else {
                        transform = ent.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
                    }

                    userData->pose = transform;
                } else if (ent.Has<ecs::TransformTree>(lock)) {
                    ent.Set<ecs::TransformSnapshot>(lock, ent.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock));
                }
            }

            for (auto ent : lock.EntitiesWith<ecs::TransformTree>()) {
                if (!ent.Has<ecs::TransformTree, ecs::TransformSnapshot>(lock) || ent.Has<ecs::Physics>(lock)) continue;
                ent.Set<ecs::TransformSnapshot>(lock, ent.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock));
            }

            constraintSystem.BreakConstraints(lock);
            physicsQuerySystem.Frame(lock);
            laserSystem.Frame(lock);
        }

        triggerSystem.Frame();
        cache.Tick(interval);

        if (stepMode) {
            stepCount++;
            stepCount.notify_all();
        }
    }

    void PhysxManager::CreatePhysxScene() {
        ZoneScoped;
        PxSceneDesc sceneDesc(pxPhysics->getTolerancesScale());

        sceneDesc.gravity = PxVec3(0.f, CVarGravity.Get(true), 0.f);
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;

        // Don't collide interactive elements with the world, or the player's body
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::Interactive, (uint16_t)ecs::PhysicsGroup::World, false);
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::Interactive, (uint16_t)ecs::PhysicsGroup::Player, false);
        // Don't collide the player with themselves, but allow the hands to collide with eachother
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::Player, (uint16_t)ecs::PhysicsGroup::Player, false);
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::Player, (uint16_t)ecs::PhysicsGroup::PlayerHands, false);
        // Don't collide anything with the noclip group.
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::NoClip, (uint16_t)ecs::PhysicsGroup::World, false);
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::NoClip, (uint16_t)ecs::PhysicsGroup::Interactive, false);
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::NoClip, (uint16_t)ecs::PhysicsGroup::Player, false);
        PxSetGroupCollisionFlag((uint16_t)ecs::PhysicsGroup::NoClip, (uint16_t)ecs::PhysicsGroup::PlayerHands, false);

        dispatcher = PxDefaultCpuDispatcherCreate(1);
        sceneDesc.cpuDispatcher = dispatcher;

        auto pxScene = pxPhysics->createScene(sceneDesc);
        Assert(pxScene, "Failed to create PhysX scene");
        scene = std::shared_ptr<PxScene>(pxScene, [](PxScene *s) {
            s->release();
        });

        auto pxControllerManager = PxCreateControllerManager(*scene);
        controllerManager = std::shared_ptr<PxControllerManager>(pxControllerManager, [](PxControllerManager *m) {
            m->purgeControllers();
            m->release();
        });

        // PxMaterial *groundMat = pxPhysics->createMaterial(0.6f, 0.5f, 0.0f);
        // PxRigidStatic *groundPlane = PxCreatePlane(*pxPhysics, PxPlane(0.f, 1.f, 0.f, 1.03f), *groundMat);

        // SetCollisionGroup(groundPlane, ecs::PhysicsGroup::World);

        // scene->addActor(*groundPlane);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            physicsObserver = lock.Watch<ecs::ComponentEvent<ecs::Physics>>();
        }
    }

    std::shared_ptr<PxConvexMesh> PhysxManager::CreateConvexMeshFromHull(const Model &model, const ConvexHull &hull) {
        PxConvexMeshDesc convexDesc;
        convexDesc.points.count = hull.pointCount;
        convexDesc.points.stride = hull.pointByteStride;
        convexDesc.points.data = hull.points;
        convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

        PxDefaultMemoryOutputStream buf;
        PxConvexMeshCookingResult::Enum result;

        if (!pxCooking->cookConvexMesh(convexDesc, buf, &result)) {
            Abortf("Failed to cook PhysX hull for %s", model.name);
        }

        PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
        PxConvexMesh *pxhull = pxPhysics->createConvexMesh(input);
        pxhull->acquireReference();
        return std::shared_ptr<PxConvexMesh>(pxhull, [](PxConvexMesh *ptr) {
            ptr->release();
        });
    }

    std::shared_ptr<const ConvexHullSet> PhysxManager::LoadConvexHullSet(const Model &model, bool decomposeHull) {
        Assert(!model.name.empty(), "PhysxManager::LoadConvexHullSet called with invalid model");
        std::string name = model.name;
        if (decomposeHull) name += "-decompose";

        auto set = cache.Load(name);
        if (!set) {
            {
                std::lock_guard lock(cacheMutex);
                // Check again in case an inflight set just completed on another thread
                set = cache.Load(name);
                if (set) return set;

                set = std::make_shared<ConvexHullSet>();
                cache.Register(name, set);
            }
            workQueue.Dispatch<void>([this, set, name, &model, decomposeHull]() {
                ZoneScopedN("LoadConvexHullSet::Dispatch");
                ZoneStr(name);
                model.WaitUntilValid();
                if (LoadCollisionCache(*set, model, decomposeHull)) {
                    for (auto &hull : set->hulls) {
                        hull.pxMesh = CreateConvexMeshFromHull(model, hull);
                    }
                    set->valid.test_and_set();
                    set->valid.notify_all();
                    return;
                }

                ConvexHullBuilding::BuildConvexHulls(set.get(), model, decomposeHull);
                for (auto &hull : set->hulls) {
                    hull.pxMesh = CreateConvexMeshFromHull(model, hull);
                }
                set->valid.test_and_set();
                set->valid.notify_all();
                SaveCollisionCache(model, *set, decomposeHull);
            });
        }

        return set;
    }

    PxRigidActor *PhysxManager::CreateActor(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::Physics>> lock,
        Tecs::Entity &e) {
        auto &ph = e.Get<ecs::Physics>(lock);
        if (!ph.model) return nullptr;

        auto &transform = e.Get<ecs::TransformTree>(lock);
        auto globalTransform = transform.GetGlobalTransform(lock);
        auto scale = globalTransform.GetScale();

        auto pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform.GetPosition()),
            GlmQuatToPxQuat(globalTransform.GetRotation()));

        PxRigidActor *actor = nullptr;
        if (ph.dynamic) {
            actor = pxPhysics->createRigidDynamic(pxTransform);

            if (ph.kinematic) { actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true); }
        } else {
            actor = pxPhysics->createRigidStatic(pxTransform);
        }
        Assert(actor, "Physx did not return valid PxRigidActor");

        auto userData = new ActorUserData(e, globalTransform, ph.group);
        actor->userData = userData;

        PxMaterial *mat = pxPhysics->createMaterial(0.6f, 0.5f, 0.0f);

        userData->shapeCache = LoadConvexHullSet(*ph.model.get(), ph.decomposeHull);
        userData->shapeCache->WaitUntilValid();

        for (auto &hull : userData->shapeCache->hulls) {
            PxRigidActorExt::createExclusiveShape(*actor,
                PxConvexMeshGeometry(hull.pxMesh.get(), PxMeshScale(GlmVec3ToPxVec3(scale))),
                *mat);
        }

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic) PxRigidBodyExt::updateMassAndInertia(*dynamic, ph.density);

        SetCollisionGroup(actor, ph.group);
        return actor;
    }

    void PhysxManager::UpdateActor(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::Physics>> lock, Tecs::Entity &e) {
        if (actors.count(e) == 0) {
            auto actor = CreateActor(lock, e);
            if (!actor) return;
            actors[e] = actor;
        }
        auto &actor = actors[e];
        if (!actor->getScene()) scene->addActor(*actor);

        auto &ph = e.Get<ecs::Physics>(lock);
        auto transform = e.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
        auto userData = (ActorUserData *)actor->userData;
        if (transform.matrix != userData->pose.matrix) {
            glm::vec3 scale = transform.GetScale();
            bool scaleChanged = scale != userData->scale;
            if (scaleChanged) {
                auto n = actor->getNbShapes();
                std::vector<PxShape *> shapes(n);
                actor->getShapes(&shapes[0], n);
                for (uint32 i = 0; i < n; i++) {
                    PxConvexMeshGeometry geom;
                    if (shapes[i]->getConvexMeshGeometry(geom)) {
                        geom.scale = PxMeshScale(GlmVec3ToPxVec3(scale));
                        shapes[i]->setGeometry(geom);
                    } else {
                        Abort("Physx geometry type not implemented");
                    }
                }
                userData->scale = scale;
            }

            PxTransform pxTransform(GlmVec3ToPxVec3(transform.GetPosition()), GlmQuatToPxQuat(transform.GetRotation()));
            auto dynamic = actor->is<PxRigidDynamic>();
            if (dynamic) {
                if (scaleChanged) PxRigidBodyExt::updateMassAndInertia(*dynamic, ph.density);
                if (ph.kinematic) {
                    dynamic->setKinematicTarget(pxTransform);
                } else {
                    actor->setGlobalPose(pxTransform);
                }
            } else {
                actor->setGlobalPose(pxTransform);
            }

            userData->velocity = transform.GetPosition() - userData->pose.GetPosition();
            userData->pose = transform;
        }
        if (userData->physicsGroup != ph.group) SetCollisionGroup(actor, ph.group);
    }

    void PhysxManager::RemoveActor(PxRigidActor *actor) {
        if (actor) {
            if (actor->userData) {
                delete (ActorUserData *)actor->userData;
                actor->userData = nullptr;
            }

            if (actor->getScene()) actor->getScene()->removeActor(*actor);
            actor->release();
        }
    }

    void PhysxManager::SetCollisionGroup(physx::PxRigidActor *actor, ecs::PhysicsGroup group) {
        PxU32 nShapes = actor->getNbShapes();
        vector<PxShape *> shapes(nShapes);
        actor->getShapes(shapes.data(), nShapes);

        PxFilterData queryFilter, simulationFilter;
        queryFilter.word0 = 1 << (size_t)group;
        simulationFilter.word0 = (uint32_t)group;
        for (uint32 i = 0; i < nShapes; ++i) {
            shapes[i]->setQueryFilterData(queryFilter);
            shapes[i]->setSimulationFilterData(simulationFilter);
        }

        auto userData = (ActorUserData *)actor->userData;
        if (userData) userData->physicsGroup = group;
    }

    // Increment if the Collision Cache format ever changes
    const uint32 hullCacheMagic = 0xc042;

    bool PhysxManager::LoadCollisionCache(ConvexHullSet &set, const Model &model, bool decomposeHull) {
        ZoneScoped;
        std::ifstream in;

        std::string path = "cache/collision/" + model.name;
        if (decomposeHull) path += "-decompose";

        if (GAssets.InputStream(path, AssetType::Bundled, in)) {
            uint32 magic;
            in.read((char *)&magic, 4);
            if (magic != hullCacheMagic) {
                Logf("Ignoring outdated collision cache format for %s", path);
                in.close();
                return false;
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
                    return false;
                }
            }

            int32 hullCount;
            in.read((char *)&hullCount, 4);
            Assert(hullCount > 0, "hull cache count");

            set.hulls.reserve(hullCount);

            for (int i = 0; i < hullCount; i++) {
                auto &hull = set.hulls.emplace_back();
                in.read((char *)&hull, 4 * sizeof(uint32));

                Assert(hull.pointByteStride % sizeof(float) == 0, "convex hull byte stride is odd");
                Assert(hull.triangleByteStride % sizeof(int) == 0, "convex hull byte stride is odd");

                hull.points = new float[hull.pointCount * hull.pointByteStride / sizeof(float)];
                hull.triangles = new int[hull.triangleCount * hull.triangleByteStride / sizeof(int)];

                in.read((char *)hull.points, hull.pointCount * hull.pointByteStride);
                in.read((char *)hull.triangles, hull.triangleCount * hull.triangleByteStride);
            }

            in.close();
            return true;
        }

        return false;
    }

    void PhysxManager::SaveCollisionCache(const Model &model, const ConvexHullSet &set, bool decomposeHull) {
        std::ofstream out;
        std::string name = "cache/collision/" + model.name;
        if (decomposeHull) name += "-decompose";

        if (GAssets.OutputStream(name, out)) {
            out.write((char *)&hullCacheMagic, 4);

            int32 bufferCount = set.bufferIndexes.size();
            out.write((char *)&bufferCount, 4);

            for (int bufferIndex : set.bufferIndexes) {
                Hash128 hash = model.HashBuffer(bufferIndex);
                string bufferName = std::to_string(bufferIndex);
                uint32 nameLen = bufferName.length();
                Assert(nameLen <= 256, "hull cache buffer name too long on write");

                out.write((char *)&nameLen, 4);
                out.write(bufferName.c_str(), nameLen);
                out.write((char *)hash.data(), sizeof(hash));
            }

            int32 hullCount = set.hulls.size();
            out.write((char *)&hullCount, 4);

            for (auto hull : set.hulls) {
                out.write((char *)&hull, 4 * sizeof(uint32));
                out.write((char *)hull.points, hull.pointCount * hull.pointByteStride);
                out.write((char *)hull.triangles, hull.triangleCount * hull.triangleByteStride);
            }

            out.close();
        }
    }
} // namespace sp
