#include "PhysxManager.hh"

#include "PhysxUtils.hh"
#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "console/CFunc.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"

#include <PxScene.h>
#include <chrono>
#include <cmath>
#include <fstream>

namespace sp {
    using namespace physx;

    // clang-format off
    CVar<float> CVarGravity("x.Gravity", -9.81f, "Acceleration due to gravity (m/sec^2)");
    // clang-format on

    PhysxManager::PhysxManager()
        : RegisteredThread("PhysX", 120.0, true), characterControlSystem(*this), constraintSystem(*this),
          physicsQuerySystem(*this), laserSystem(*this), animationSystem(*this) {
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
            ZoneScopedN("Sync from ECS");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
                                                        ecs::SignalOutput,
                                                        ecs::SignalBindings,
                                                        ecs::FocusLayer,
                                                        ecs::FocusLock,
                                                        ecs::Physics>,
                ecs::Write<ecs::Animation, ecs::Physics, ecs::Transform>>();

            // Delete actors for removed entities
            ecs::ComponentEvent<ecs::Physics> physicsEvent;
            while (physicsObserver.Poll(lock, physicsEvent)) {
                if (physicsEvent.type == Tecs::EventType::REMOVED && physicsEvent.component.actor) {
                    RemoveActor(physicsEvent.component.actor);
                }
            }

            // Update actors with latest entity data
            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::Transform>(lock)) continue;

                auto &ph = ent.Get<ecs::Physics>(lock);
                if (ph.model && !ph.model->Valid()) {
                    // Not all actors are ready, skip this frame.
                    return;
                }

                if (!ph.actor) {
                    CreateActor(lock, ent);
                } else {
                    UpdateActor(lock, ent);
                }
            }

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
                                                        ecs::Physics,
                                                        ecs::Mirror>,
                ecs::Write<ecs::Animation,
                    ecs::CharacterController,
                    ecs::Transform,
                    ecs::EventInput,
                    ecs::PhysicsQuery,
                    ecs::LaserLine,
                    ecs::LaserSensor,
                    ecs::SignalOutput>>();

            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::Transform>(lock)) continue;

                auto &ph = ent.Get<ecs::Physics>(lock);

                if (!ph.dynamic || ph.kinematic) continue;

                if (ph.actor) {
                    auto &readTransform = ent.Get<const ecs::Transform>(lock);

                    auto userData = (ActorUserData *)ph.actor->userData;
                    if (!readTransform.HasChanged(userData->transformChangeNumber)) {
                        auto &transform = ent.Get<ecs::Transform>(lock);

                        auto pose = ph.actor->getGlobalPose();
                        transform.SetPosition(PxVec3ToGlmVec3(pose.p));
                        transform.SetRotation(PxQuatToGlmQuat(pose.q));
                        transform.SetParent(Tecs::Entity());

                        userData->transformChangeNumber = transform.ChangeNumber();
                    }
                }
            }

            characterControlSystem.Frame(lock);
            physicsQuerySystem.Frame(lock);
            laserSystem.Frame(lock);
            animationSystem.Frame(lock);
        }

        triggerSystem.Frame();
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

        controllerHitReporter = make_unique<ControllerHitReport>();

        auto pxControllerManager = PxCreateControllerManager(*scene);
        controllerManager = std::shared_ptr<PxControllerManager>(pxControllerManager, [](PxControllerManager *m) {
            m->purgeControllers();
            m->release();
        });

        PxMaterial *groundMat = pxPhysics->createMaterial(0.6f, 0.5f, 0.0f);
        PxRigidStatic *groundPlane = PxCreatePlane(*pxPhysics, PxPlane(0.f, 1.f, 0.f, 1.03f), *groundMat);

        SetCollisionGroup(groundPlane, ecs::PhysicsGroup::World);

        scene->addActor(*groundPlane);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            physicsObserver = lock.Watch<ecs::ComponentEvent<ecs::Physics>>();
        }
    }

    ConvexHullSet *PhysxManager::GetCachedConvexHulls(std::string name) {
        if (cache.count(name)) { return cache[name]; }

        return nullptr;
    }

    ConvexHullSet *PhysxManager::BuildConvexHulls(const Model &model, bool decomposeHull) {
        ZoneScoped;
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

    void PhysxManager::CreateActor(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::Physics>> lock,
        Tecs::Entity &e) {
        auto &ph = e.Get<ecs::Physics>(lock);
        auto &transform = e.Get<ecs::Transform>(lock);

        if (ph.actor || !ph.model || !ph.model->Valid()) return;

        auto globalTransform = transform.GetGlobalTransform(lock);
        auto scale = globalTransform.GetScale();

        auto pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform.GetPosition()),
            GlmQuatToPxQuat(globalTransform.GetRotation()));

        if (ph.dynamic) {
            ph.actor = pxPhysics->createRigidDynamic(pxTransform);

            if (ph.kinematic) { ph.actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true); }
        } else {
            ph.actor = pxPhysics->createRigidStatic(pxTransform);
        }
        Assert(ph.actor, "Physx did not return valid PxRigidActor");

        ph.actor->userData = new ActorUserData(e, transform.ChangeNumber(), ph.group);

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

            PxRigidActorExt::createExclusiveShape(*ph.actor,
                PxConvexMeshGeometry(pxhull, PxMeshScale(GlmVec3ToPxVec3(scale))),
                *mat);
        }

        auto dynamic = ph.actor->is<PxRigidDynamic>();
        if (dynamic) {
            PxRigidBodyExt::updateMassAndInertia(*dynamic, ph.density);
            ph.centerOfMass = PxVec3ToGlmVec3(dynamic->getCMassLocalPose().p);
        }

        SetCollisionGroup(ph.actor, ph.group);

        scene->addActor(*ph.actor);
    }

    void PhysxManager::UpdateActor(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::Physics>> lock,
        Tecs::Entity &e) {
        auto &ph = e.Get<ecs::Physics>(lock);

        if (!ph.actor || !ph.model || !ph.model->Valid()) return;

        auto &transform = e.Get<ecs::Transform>(lock);

        PxTransform pxTransform;
        glm::vec3 scale;
        auto userData = (ActorUserData *)ph.actor->userData;
        bool transformChanged = transform.HasChanged(userData->transformChangeNumber);
        bool scaleChanged = false;

        if (transformChanged) {
            auto globalTransform = transform.GetGlobalTransform(lock);
            pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform.GetPosition()),
                GlmQuatToPxQuat(globalTransform.GetRotation()));
            scale = globalTransform.GetScale();
            scaleChanged = ph.scale != scale;
        } else if (transform.HasParent(lock)) {
            auto globalTransform = transform.GetGlobalTransform(lock);
            pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform.GetPosition()),
                GlmQuatToPxQuat(globalTransform.GetRotation()));
            scale = globalTransform.GetScale();

            auto globalPose = ph.actor->getGlobalPose();
            scaleChanged = ph.scale != scale;
            transformChanged = globalPose != pxTransform || scaleChanged;
        }

        if (transformChanged) {
            if (scaleChanged) {
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

            auto dynamic = ph.actor->is<PxRigidDynamic>();
            if (dynamic) {
                if (scaleChanged) {
                    PxRigidBodyExt::updateMassAndInertia(*dynamic, ph.density);
                    ph.centerOfMass = PxVec3ToGlmVec3(dynamic->getCMassLocalPose().p);
                }
                if (ph.kinematic) {
                    dynamic->setKinematicTarget(pxTransform);
                } else {
                    ph.actor->setGlobalPose(pxTransform);
                }
            } else {
                ph.actor->setGlobalPose(pxTransform);
            }

            userData->transformChangeNumber = transform.ChangeNumber();
        }
        if (userData->currentPhysicsGroup != ph.group) SetCollisionGroup(ph.actor, ph.group);
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
        if (userData) userData->currentPhysicsGroup = group;
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
