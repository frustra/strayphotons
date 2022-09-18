#include "PhysxManager.hh"

#include "PhysxUtils.hh"
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "assets/JsonHelpers.hh"
#include "assets/PhysicsInfo.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <PxScene.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <glm/ext/matrix_relational.hpp>
#include <murmurhash/MurmurHash3.h>

namespace sp {
    using namespace physx;

    CVar<bool> CVarPhysxDebugCollision("x.DebugColliders", false, "Show physx colliders");
    CVar<bool> CVarPhysxDebugJoints("x.DebugJoints", false, "Show physx joints");

    PhysxManager::PhysxManager(bool stepMode)
        : RegisteredThread("PhysX", 120.0, true), scenes(GetSceneManager()), characterControlSystem(*this),
          constraintSystem(*this), physicsQuerySystem(*this), laserSystem(*this), animationSystem(*this),
          workQueue("PhysXHullLoading") {
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

        pxSerialization = PxSerialization::createSerializationRegistry(*pxPhysics);

        scratchBlock.resize(0x1000000); // 16MiB

        CreatePhysxScene();
        if (stepMode) {
            funcs.Register<unsigned int>("stepphysics",
                "Advance the physics simulation by N frames, default is 1",
                [this](unsigned int arg) {
                    this->Step(std::max(1u, arg));
                });
        }

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "physx",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, debugLineEntity.Name());
                auto &laser = ent.Set<ecs::LaserLine>(lock);
                laser.intensity = 0.5f;
                laser.mediaDensityFactor = 0;
                laser.radius = 0.005f;
                laser.line = ecs::LaserLine::Segments();
            });

        StartThread(stepMode);
    }

    PhysxManager::~PhysxManager() {
        StopThread();

        controllerManager.reset();
        for (auto &entry : joints) {
            for (auto &joint : entry.second) {
                joint.pxJoint->release();
            }
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

        if (pxSerialization) {
            pxSerialization->release();
            pxSerialization = nullptr;
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

    void PhysxManager::PreFrame() {
        scenes.PreloadScenePhysics([this](auto lock, auto scene) {
            bool complete = true;
            for (auto ent : lock.template EntitiesWith<ecs::Physics>()) {
                if (!ent.template Has<ecs::SceneInfo, ecs::Physics>(lock)) continue;
                if (ent.template Get<ecs::SceneInfo>(lock).scene.lock() != scene) continue;

                auto &ph = ent.template Get<ecs::Physics>(lock);
                for (auto &shape : ph.shapes) {
                    auto mesh = std::get_if<ecs::PhysicsShape::ConvexMesh>(&shape.shape);
                    if (!mesh || !mesh->model || !mesh->hullSettings) continue;
                    if (mesh->model->Ready() && mesh->hullSettings->Ready()) {
                        auto set = LoadConvexHullSet(mesh->model, mesh->hullSettings);
                        if (!set || !set->Ready()) complete = false;
                    } else {
                        complete = false;
                    }
                }
            }
            return complete;
        });
    }

    void PhysxManager::Frame() {
        if (CVarPhysxDebugCollision.Changed() || CVarPhysxDebugJoints.Changed()) {
            bool collision = CVarPhysxDebugCollision.Get(true);
            bool joints = CVarPhysxDebugJoints.Get(true);
            scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, collision || joints ? 1 : 0);
            scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, collision);
            scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LOCAL_FRAMES, joints);
            scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LIMITS, joints);
        }

        animationSystem.Frame();

        { // Sync ECS state to physx
            ZoneScopedN("Sync from ECS");
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock,
                ecs::Read<ecs::Physics, ecs::PhysicsJoints, ecs::EventInput, ecs::SceneInfo>,
                ecs::Write<ecs::Animation, ecs::TransformTree, ecs::CharacterController>>();

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
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock,
                ecs::Read<ecs::LaserEmitter,
                    ecs::OpticalElement,
                    ecs::EventBindings,
                    ecs::EventInput,
                    ecs::CharacterController>,
                ecs::Write<ecs::Animation,
                    ecs::TransformSnapshot,
                    ecs::TransformTree,
                    ecs::Physics,
                    ecs::PhysicsQuery,
                    ecs::LaserLine,
                    ecs::LaserSensor,
                    ecs::SignalOutput,
                    ecs::Script>,
                ecs::PhysicsUpdateLock>();

            {
                ZoneScopedN("UpdateSnapshots(DynamicPhysics)");
                for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                    if (!ent.Has<ecs::Physics, ecs::TransformSnapshot, ecs::TransformTree>(lock)) continue;

                    auto &ph = ent.Get<ecs::Physics>(lock);
                    if (actors.count(ent) > 0) {
                        auto const &actor = actors[ent];
                        auto &transform = ent.Get<ecs::TransformSnapshot>(lock);

                        auto userData = (ActorUserData *)actor->userData;
                        Assert(userData, "Physics actor is missing UserData");
                        if (ph.dynamic && !ph.kinematic && transform == userData->pose) {
                            auto pose = actor->getGlobalPose();
                            transform.SetPosition(PxVec3ToGlmVec3(pose.p));
                            transform.SetRotation(PxQuatToGlmQuat(pose.q));
                            ent.Set<ecs::TransformTree>(lock, transform);
                            userData->velocity = transform.GetPosition() - userData->pose.GetPosition();
                            userData->pose = transform;
                        }
                    }
                }
            }

            {
                ZoneScopedN("UpdateSnapshots(NonPhysics)");
                for (auto ent : lock.EntitiesWith<ecs::TransformTree>()) {
                    if (!ent.Has<ecs::TransformTree, ecs::TransformSnapshot>(lock)) continue;
                    auto transform = ent.Get<const ecs::TransformTree>(lock).GetGlobalTransform(lock);
                    ent.Set<ecs::TransformSnapshot>(lock, transform);

                    if (ent.Has<ecs::Physics>(lock)) {
                        auto &ph = ent.Get<ecs::Physics>(lock);
                        if (ph.dynamic && !ph.kinematic) continue;

                        if (actors.count(ent) > 0) {
                            auto const &actor = actors[ent];
                            auto userData = (ActorUserData *)actor->userData;
                            Assert(userData, "Physics actor is missing UserData");

                            if (transform != userData->pose) {
                                PxTransform pxTransform(GlmVec3ToPxVec3(transform.GetPosition()),
                                    GlmQuatToPxQuat(transform.GetRotation()));
                                auto dynamic = actor->is<PxRigidDynamic>();
                                if (dynamic && ph.kinematic) {
                                    dynamic->setKinematicTarget(pxTransform);
                                } else {
                                    actor->setGlobalPose(pxTransform);
                                }
                            }
                        }
                    }
                }
            }

            constraintSystem.BreakConstraints(lock);
            physicsQuerySystem.Frame(lock);
            laserSystem.Frame(lock);

            {
                ZoneScopedN("Script::OnPhysicsUpdate");
                for (auto &entity : lock.EntitiesWith<ecs::Script>()) {
                    auto &script = entity.Get<ecs::Script>(lock);
                    script.OnPhysicsUpdate(lock, entity, interval);
                }
            }

            auto debugLines = debugLineEntity.Get(lock);
            if (debugLines.Has<ecs::LaserLine>(lock)) {
                auto &laser = debugLines.Get<ecs::LaserLine>(lock);
                auto &segments = std::get<ecs::LaserLine::Segments>(laser.line);
                segments.clear();
                if (CVarPhysxDebugCollision.Get() || CVarPhysxDebugJoints.Get()) {
                    auto &rb = scene->getRenderBuffer();
                    for (size_t i = 0; i < rb.getNbLines(); i++) {
                        auto &line = rb.getLines()[i];
                        ecs::LaserLine::Segment segment;
                        segment.start = PxVec3ToGlmVec3(line.pos0);
                        segment.end = PxVec3ToGlmVec3(line.pos1);
                        segment.color = PxColorToGlmVec3(line.color0);
                        segments.push_back(segment);
                    }
                    for (size_t i = 0; i < rb.getNbTriangles(); i++) {
                        auto &triangle = rb.getTriangles()[i];
                        ecs::LaserLine::Segment segment;
                        segment.start = PxVec3ToGlmVec3(triangle.pos0);
                        segment.end = PxVec3ToGlmVec3(triangle.pos1);
                        segment.color = PxColorToGlmVec3(triangle.color0);
                        segments.push_back(segment);
                        segment.start = PxVec3ToGlmVec3(triangle.pos1);
                        segment.end = PxVec3ToGlmVec3(triangle.pos2);
                        segment.color = PxColorToGlmVec3(triangle.color1);
                        segments.push_back(segment);
                        segment.start = PxVec3ToGlmVec3(triangle.pos2);
                        segment.end = PxVec3ToGlmVec3(triangle.pos0);
                        segment.color = PxColorToGlmVec3(triangle.color2);
                        segments.push_back(segment);
                    }
                }
            }
        }

        triggerSystem.Frame();
        cache.Tick(interval);
    }

    void PhysxManager::CreatePhysxScene() {
        ZoneScoped;
        PxSceneDesc sceneDesc(pxPhysics->getTolerancesScale());

        sceneDesc.gravity = PxVec3(0); // Gravity handled by scene properties
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;

        using Group = ecs::PhysicsGroup;
        // Don't collide world overlap elements with the world
        PxSetGroupCollisionFlag((uint16_t)Group::WorldOverlap, (uint16_t)Group::World, false);
        PxSetGroupCollisionFlag((uint16_t)Group::WorldOverlap, (uint16_t)Group::WorldOverlap, false);
        PxSetGroupCollisionFlag((uint16_t)Group::WorldOverlap, (uint16_t)Group::Interactive, false);
        // Don't collide the player with themselves, but allow the hands to collide with eachother
        PxSetGroupCollisionFlag((uint16_t)Group::Player, (uint16_t)Group::Player, false);
        PxSetGroupCollisionFlag((uint16_t)Group::Player, (uint16_t)Group::PlayerLeftHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::Player, (uint16_t)Group::PlayerRightHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::PlayerLeftHand, (uint16_t)Group::PlayerLeftHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::PlayerRightHand, (uint16_t)Group::PlayerRightHand, false);
        // Don't collide user interface elements with objects in the world or other interfaces
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::World, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::WorldOverlap, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::Interactive, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::Player, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::UserInterface, false);
        // Don't collide anything with the noclip group.
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::NoClip, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::World, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::WorldOverlap, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::Interactive, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::Player, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::PlayerLeftHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::PlayerRightHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::UserInterface, false);

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
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            physicsObserver = lock.Watch<ecs::ComponentEvent<ecs::Physics>>();
        }
    }

    AsyncPtr<ConvexHullSet> PhysxManager::LoadConvexHullSet(AsyncPtr<Gltf> modelPtr,
        AsyncPtr<HullSettings> settingsPtr) {
        Assertf(modelPtr, "PhysxManager::LoadConvexHullSet called with null model ptr");
        Assertf(settingsPtr, "PhysxManager::LoadConvexHullSet called with null hull settings ptr");
        auto model = modelPtr->Get();
        auto settings = settingsPtr->Get();
        Assertf(model, "PhysxManager::LoadConvexHullSet called with null model");
        Assertf(settings, "PhysxManager::LoadConvexHullSet called with null hull settings");

        Assertf(!settings->name.empty(), "PhysxManager::LoadConvexHullSet called with invalid hull settings");
        auto set = cache.Load(settings->name);
        if (!set) {
            {
                std::lock_guard lock(cacheMutex);
                // Check again in case an inflight set just completed on another thread
                set = cache.Load(settings->name);
                if (set) return set;

                set = workQueue.Dispatch<ConvexHullSet>([this, modelPtr, settingsPtr, name = settings->name]() {
                    ZoneScopedN("LoadConvexHullSet::Dispatch");
                    ZoneStr(name);

                    auto set = hullgen::LoadCollisionCache(*pxSerialization, modelPtr, settingsPtr);
                    if (set) return set;

                    set = hullgen::BuildConvexHulls(*pxCooking, *pxPhysics, modelPtr, settingsPtr);
                    hullgen::SaveCollisionCache(*pxSerialization, modelPtr, settingsPtr, *set);

                    return set;
                });
                cache.Register(settings->name, set);
            }
        }

        return set;
    }

    PxRigidActor *PhysxManager::CreateActor(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::Physics>> lock,
        ecs::Entity &e) {
        ZoneScoped;
        auto &ph = e.Get<ecs::Physics>(lock);
        if (ph.shapes.empty()) return nullptr;

        const ecs::PhysicsShape::ConvexMesh *mesh = nullptr;
        for (auto &shape : ph.shapes) {
            if (mesh) Abortf("Physics actor can't have multiple meshes: %s", std::to_string(e));
            mesh = std::get_if<ecs::PhysicsShape::ConvexMesh>(&shape.shape);
            if (mesh && (!mesh->model || !mesh->hullSettings)) return nullptr;
        }

        auto &transform = e.Get<ecs::TransformTree>(lock);
        auto globalTransform = transform.GetGlobalTransform(lock);
        auto scale = globalTransform.GetScale();

        auto pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform.GetPosition()),
            GlmQuatToPxQuat(globalTransform.GetRotation()));

        PxRigidActor *actor = nullptr;
        if (ph.dynamic) {
            actor = pxPhysics->createRigidDynamic(pxTransform);

            if (ph.kinematic) {
                actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
                actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES,
                    true);
            }
        } else {
            actor = pxPhysics->createRigidStatic(pxTransform);
        }
        Assert(actor, "Physx did not return valid PxRigidActor");

        actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);

        auto userData = new ActorUserData(e, globalTransform, ph.group);
        actor->userData = userData;

        userData->material = std::shared_ptr<PxMaterial>(pxPhysics->createMaterial(0.6f, 0.5f, 0.0f), [](auto *ptr) {
            ptr->release();
        });

        userData->shapeIndexes.clear();
        if (mesh) {
            userData->shapeCache = LoadConvexHullSet(mesh->model, mesh->hullSettings)->Get();

            if (userData->shapeCache) {
                for (auto &hull : userData->shapeCache->hulls) {
                    PxRigidActorExt::createExclusiveShape(*actor,
                        PxConvexMeshGeometry(hull.get(), PxMeshScale(GlmVec3ToPxVec3(scale))),
                        *userData->material);
                }
            } else {
                Errorf("Physics actor created with invalid mesh: %s", mesh->meshName);
            }
        } else {
            userData->shapeIndexes.reserve(ph.shapes.size());
            for (auto &shape : ph.shapes) {
                PxShape *pxShape = PxRigidActorExt::createExclusiveShape(*actor,
                    GeometryFromShape(shape, scale).any(),
                    *userData->material);
                Assertf(pxShape, "Failed to create physx shape");

                PxTransform shapeTransform(GlmVec3ToPxVec3(shape.transform.GetPosition()),
                    GlmQuatToPxQuat(shape.transform.GetRotation()));
                pxShape->setLocalPose(shapeTransform);

                userData->shapeIndexes.emplace_back(shape, userData->shapeIndexes.size());
            }
        }

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic) {
            PxRigidBodyExt::updateMassAndInertia(*dynamic, ph.density);
            dynamic->setAngularDamping(ph.angularDamping);
            dynamic->setLinearDamping(ph.linearDamping);

            userData->angularDamping = ph.angularDamping;
            userData->linearDamping = ph.linearDamping;
        }

        SetCollisionGroup(actor, ph.group);
        scene->addActor(*actor);
        return actor;
    }

    void PhysxManager::UpdateActor(
        ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree, ecs::Physics, ecs::SceneInfo>> lock,
        ecs::Entity &e) {
        ZoneScoped;
        // ZoneStr(ecs::ToString(lock, e));
        if (actors.count(e) == 0) {
            auto actor = CreateActor(lock, e);
            if (actor) actors[e] = actor;
            return;
        }
        auto &actor = actors[e];
        auto dynamic = actor->is<PxRigidDynamic>();

        auto &ph = e.Get<ecs::Physics>(lock);
        auto transform = e.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
        auto scale = transform.GetScale();
        auto userData = (ActorUserData *)actor->userData;

        ecs::SceneProperties sceneProperties = {};
        if (e.Has<ecs::SceneInfo>(lock)) {
            auto &properties = e.Get<ecs::SceneInfo>(lock).properties;
            if (properties) sceneProperties = *properties;
        }

        const ecs::PhysicsShape::ConvexMesh *mesh = nullptr;
        for (auto &shape : ph.shapes) {
            if (mesh) Abortf("Physics actor can't have multiple meshes: %s", ecs::ToString(lock, e));
            mesh = std::get_if<ecs::PhysicsShape::ConvexMesh>(&shape.shape);
            if (mesh && !mesh->model) return;
        }

        bool shapesChanged = false;
        auto actorShapeCount = actor->getNbShapes();
        if (mesh) {
            if (glm::any(glm::notEqual(scale, userData->scale, 1e-5f)) && actorShapeCount > 0) {
                // Logf("Updating actor mesh: %s", ecs::ToString(lock, e));
                std::vector<PxShape *> pxShapes(actorShapeCount);
                actor->getShapes(&pxShapes[0], actorShapeCount);
                PxConvexMeshGeometry meshGeom;
                for (auto *pxShape : pxShapes) {
                    if (pxShape->getConvexMeshGeometry(meshGeom)) {
                        meshGeom.scale = PxMeshScale(GlmVec3ToPxVec3(scale));
                        pxShape->setGeometry(meshGeom);
                    } else {
                        actor->detachShape(*pxShape);
                    }
                }
                shapesChanged = true;
            }
            userData->scale = scale;
        } else {
            // First check if any shapes were added or removed, and if the shape types match
            if (ph.shapes.size() == userData->shapeIndexes.size()) {
                for (size_t i = 0; i < ph.shapes.size(); i++) {
                    if (ph.shapes[i].shape.index() != userData->shapeIndexes[i].first.shape.index() ||
                        userData->shapeIndexes[i].second >= actorShapeCount) {
                        shapesChanged = true;
                        break;
                    }
                }
            } else {
                shapesChanged = true;
            }

            if (shapesChanged) {
                ZoneScopedN("ShapesChanged");
                ZoneStr(ecs::ToString(lock, e));

                std::vector<PxShape *> pxShapes(actorShapeCount);
                if (actorShapeCount > 0) {
                    actor->getShapes(&pxShapes[0], actorShapeCount);
                    for (auto *shape : pxShapes) {
                        actor->detachShape(*shape);
                    }
                }

                userData->shapeIndexes.clear();
                for (auto &shape : ph.shapes) {
                    auto geometry = GeometryFromShape(shape, scale);

                    auto *pxShape = PxRigidActorExt::createExclusiveShape(*actor, geometry.any(), *userData->material);
                    Assertf(pxShape, "Failed to create physx shape");

                    PxTransform shapeTransform(GlmVec3ToPxVec3(shape.transform.GetPosition()),
                        GlmQuatToPxQuat(shape.transform.GetRotation()));
                    pxShape->setLocalPose(shapeTransform);

                    userData->shapeIndexes.emplace_back(shape, userData->shapeIndexes.size());
                }

                SetCollisionGroup(actor, ph.group);
            } else if (actorShapeCount > 0) {
                ZoneScopedN("ShapesUpdate");

                std::vector<PxShape *> pxShapes(actorShapeCount);
                actor->getShapes(&pxShapes[0], actorShapeCount);

                for (size_t i = 0; i < ph.shapes.size(); i++) {
                    auto &shape = ph.shapes[i];
                    auto &shapeIndex = userData->shapeIndexes[i];
                    Assertf(shapeIndex.second < pxShapes.size(),
                        "PxShape index out of range: %u/%u",
                        shapeIndex.second,
                        pxShapes.size());

                    if (shape.shape != shapeIndex.first.shape) {
                        // Logf("Updating actor shape geometry: index %u", shapeIndex.second);
                        auto geometry = GeometryFromShape(shape, scale);
                        pxShapes[shapeIndex.second]->setGeometry(geometry.any());
                        shapesChanged = true;
                    }
                    if (glm::any(glm::notEqual(shape.transform.matrix, shapeIndex.first.transform.matrix, 1e-4f))) {
                        // Logf("Updating actor shape pose: index %u", shapeIndex.second);

                        PxTransform shapeTransform(GlmVec3ToPxVec3(shape.transform.GetPosition()),
                            GlmQuatToPxQuat(shape.transform.GetRotation()));
                        pxShapes[shapeIndex.second]->setLocalPose(shapeTransform);
                        shapesChanged = true;
                    }
                    userData->shapeIndexes[i] = {shape, shapeIndex.second};
                }
            }
        }

        if (dynamic && shapesChanged) PxRigidBodyExt::updateMassAndInertia(*dynamic, ph.density);
        if (glm::any(glm::notEqual(transform.matrix, userData->pose.matrix, 1e-5f))) {
            // Logf("Updating actor position: %s", ecs::ToString(lock, e));
            PxTransform pxTransform(GlmVec3ToPxVec3(transform.GetPosition()), GlmQuatToPxQuat(transform.GetRotation()));
            if (dynamic && ph.kinematic) {
                dynamic->setKinematicTarget(pxTransform);
            } else {
                actor->setGlobalPose(pxTransform);
            }

            userData->velocity = transform.GetPosition() - userData->pose.GetPosition();
        } else {
            userData->velocity = glm::vec3(0);
        }
        userData->pose = transform;
        if (userData->physicsGroup != ph.group) SetCollisionGroup(actor, ph.group);
        if (dynamic) {
            if (userData->angularDamping != ph.angularDamping) {
                dynamic->setAngularDamping(ph.angularDamping);
                userData->angularDamping = ph.angularDamping;
            }
            if (userData->linearDamping != ph.linearDamping) {
                dynamic->setLinearDamping(ph.linearDamping);
                userData->linearDamping = ph.linearDamping;
            }

            if (!dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) {
                glm::vec3 gravityForce = sceneProperties.fixedGravity;
                if (sceneProperties.gravityFunction) {
                    auto gravityPos = sceneProperties.gravityTransform.GetInverse() *
                                      glm::vec4(transform.GetPosition(), 1);
                    gravityForce = sceneProperties.gravityTransform.GetRotation() *
                                   sceneProperties.gravityFunction(gravityPos);
                }
                // Force will accumulate on sleeping objects causing jitter
                if (gravityForce != glm::vec3(0) && !dynamic->isSleeping()) {
                    dynamic->addForce(GlmVec3ToPxVec3(gravityForce), PxForceMode::eACCELERATION, false);
                }
                if (gravityForce != userData->gravity) {
                    dynamic->wakeUp();
                    userData->gravity = gravityForce;
                }
            }
        }
    }

    void PhysxManager::RemoveActor(PxRigidActor *actor) {
        if (actor) {
            auto userData = (ActorUserData *)actor->userData;

            if (actor->getScene()) actor->getScene()->removeActor(*actor);
            actor->release();

            if (userData) delete userData;
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

    PxGeometryHolder PhysxManager::GeometryFromShape(const ecs::PhysicsShape &shape, glm::vec3 parentScale) {
        auto scale = shape.transform.GetScale() * parentScale;
        return std::visit(
            [this, &scale](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same<ecs::PhysicsShape::Sphere, T>()) {
                    auto avgScale = (scale.x + scale.y + scale.z) / 3.0f;
                    return PxGeometryHolder(PxSphereGeometry(avgScale * arg.radius));
                } else if constexpr (std::is_same<ecs::PhysicsShape::Capsule, T>()) {
                    auto avgScaleYZ = (scale.y + scale.z) / 2.0f;
                    return PxGeometryHolder(PxCapsuleGeometry(avgScaleYZ * arg.radius, scale.x * arg.height * 0.5f));
                } else if constexpr (std::is_same<ecs::PhysicsShape::Box, T>()) {
                    return PxGeometryHolder(PxBoxGeometry(GlmVec3ToPxVec3(scale * arg.extents * 0.5f)));
                } else if constexpr (std::is_same<ecs::PhysicsShape::Plane, T>()) {
                    return PxGeometryHolder(PxPlaneGeometry());
                } else if constexpr (std::is_same<ecs::PhysicsShape::ConvexMesh, T>()) {
                    Errorf("PhysxManager::GeometryFromShape does not support PhysicsShape::ConvexMesh");
                    return PxGeometryHolder();
                } else {
                    Errorf("Unknown PhysicsShape type: %s", typeid(T).name());
                    return PxGeometryHolder();
                }
            },
            shape.shape);
    }
} // namespace sp
