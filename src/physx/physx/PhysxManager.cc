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
#include "physx/ForceConstraint.hh"

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
                laser.radius = 0.001f;
                laser.line = ecs::LaserLine::Segments();
            });

        RegisterDebugCommands();
        StartThread(stepMode);
    }

    PhysxManager::~PhysxManager() {
        StopThread();

        workQueue.Shutdown();

        controllerManager.reset();
        for (auto &entry : joints) {
            for (auto &joint : entry.second) {
                if (joint.pxJoint) joint.pxJoint->release();
                if (joint.forceConstraint) joint.forceConstraint->release();
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
        ZoneScoped;
        scenes.PreloadScenePhysics([this](auto lock, auto scene) {
            bool complete = true;
            for (auto ent : lock.template EntitiesWith<ecs::Physics>()) {
                if (!ent.template Has<ecs::SceneInfo, ecs::Physics>(lock)) continue;
                if (ent.template Get<ecs::SceneInfo>(lock).scene != scene) continue;

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
        ZoneScoped;
        if (CVarPhysxDebugCollision.Changed() || CVarPhysxDebugJoints.Changed()) {
            bool collision = CVarPhysxDebugCollision.Get(true);
            bool joints = CVarPhysxDebugJoints.Get(true);
            scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, collision || joints ? 1 : 0);
            scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, collision);
            scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LOCAL_FRAMES, joints);
            scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LIMITS, joints);
        }

        characterControlSystem.RegisterEvents();

        { // Sync ECS state to physx
            ZoneScopedN("Sync from ECS");
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock,
                ecs::Read<ecs::Physics, ecs::EventInput, ecs::SceneProperties>,
                ecs::Write<ecs::Animation, ecs::TransformTree, ecs::PhysicsJoints, ecs::CharacterController>>();

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

            animationSystem.Frame(lock);

            // Update actors with latest entity data
            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
                auto &ph = ent.Get<ecs::Physics>(lock);
                if (ph.type == ecs::PhysicsActorType::SubActor) continue;
                UpdateActor(lock, ent);
            }
            // Update sub actors once all parent actors are complete
            for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                if (!ent.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
                auto &ph = ent.Get<ecs::Physics>(lock);
                if (ph.type != ecs::PhysicsActorType::SubActor) continue;
                UpdateActor(lock, ent);
            }

            constraintSystem.Frame(lock);
            characterControlSystem.Frame(lock);
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
                    ecs::EventBindings,
                    ecs::Physics,
                    ecs::EventInput,
                    ecs::CharacterController>,
                ecs::Write<ecs::Animation,
                    ecs::TransformSnapshot,
                    ecs::TransformTree,
                    ecs::OpticalElement,
                    ecs::PhysicsQuery,
                    ecs::LaserLine,
                    ecs::LaserSensor,
                    ecs::SignalOutput,
                    ecs::Scripts>,
                ecs::PhysicsUpdateLock>();

            {
                ZoneScopedN("UpdateSnapshots(Dynamic)");
                for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                    if (!ent.Has<ecs::Physics, ecs::TransformSnapshot, ecs::TransformTree>(lock)) continue;

                    auto &ph = ent.Get<ecs::Physics>(lock);
                    if (actors.count(ent) > 0) {
                        auto const &actor = actors[ent];
                        auto &transform = ent.Get<ecs::TransformSnapshot>(lock);

                        auto userData = (ActorUserData *)actor->userData;
                        Assert(userData, "Physics actor is missing UserData");
                        if (ph.type == ecs::PhysicsActorType::Dynamic && transform == userData->pose) {
                            auto pose = actor->getGlobalPose();
                            transform.SetPosition(PxVec3ToGlmVec3(pose.p));
                            transform.SetRotation(PxQuatToGlmQuat(pose.q));
                            ent.Set<ecs::TransformTree>(lock, transform);
                            userData->velocity = (transform.GetPosition() - userData->pose.GetPosition()) *
                                                 (float)(1e9 / interval.count());
                            userData->pose = transform;
                        }
                    }
                }
            }

            {
                ZoneScopedN("UpdateSnapshots(NonDynamic)");
                for (auto &ent : lock.EntitiesWith<ecs::TransformTree>()) {
                    if (!ent.Has<ecs::TransformTree, ecs::TransformSnapshot>(lock)) continue;

                    // Only recalculate the transform snapshot for entities that moved.
                    auto treeEnt = ent;
                    bool dirty = false;
                    while (treeEnt.Has<ecs::TransformTree>(lock)) {
                        auto &tree = treeEnt.Get<const ecs::TransformTree>(lock);
                        auto &cache = transformCache[treeEnt];
                        treeEnt = tree.parent.Get(lock);

                        if (cache.dirty < 0) {
                            dirty = tree.pose != cache.pose || treeEnt != cache.parent;
                            if (dirty) {
                                cache.pose = tree.pose;
                                cache.parent = treeEnt;
                                cache.dirty = 1;
                                break;
                            } else {
                                cache.dirty = 0;
                            }
                        } else if (cache.dirty > 0) {
                            dirty = true;
                            break;
                        }
                    }
                    if (!dirty) continue;

                    auto transform = ent.Get<const ecs::TransformTree>(lock).GetGlobalTransform(lock);
                    ent.Set<ecs::TransformSnapshot>(lock, transform);

                    if (ent.Has<ecs::Physics>(lock)) {
                        auto &ph = ent.Get<ecs::Physics>(lock);
                        if (ph.type == ecs::PhysicsActorType::Dynamic) continue;

                        if (actors.count(ent) > 0) {
                            auto const &actor = actors[ent];
                            auto userData = (ActorUserData *)actor->userData;
                            Assert(userData, "Physics actor is missing UserData");

                            if (transform != userData->pose) {
                                PxTransform pxTransform(GlmVec3ToPxVec3(transform.GetPosition()),
                                    GlmQuatToPxQuat(transform.GetRotation()));
                                if (pxTransform.isSane()) {
                                    auto dynamic = actor->is<PxRigidDynamic>();
                                    if (dynamic && ph.type == ecs::PhysicsActorType::Kinematic) {
                                        dynamic->setKinematicTarget(pxTransform);
                                    } else {
                                        actor->setGlobalPose(pxTransform);
                                    }
                                } else {
                                    Errorf("Physics Transform Snapshot is not valid for entity: %s",
                                        ecs::ToString(lock, ent));
                                }
                            }
                        }
                    }
                }
            }

            physicsQuerySystem.Frame(lock);
            laserSystem.Frame(lock);

            {
                ZoneScopedN("Scripts::OnPhysicsUpdate");
                for (auto &entity : lock.EntitiesWith<ecs::Scripts>()) {
                    auto &scripts = entity.Get<ecs::Scripts>(lock);
                    scripts.OnPhysicsUpdate(lock, entity, interval);
                }
            }

            auto debugLines = debugLineEntity.Get(lock);
            if (debugLines.Has<ecs::LaserLine>(lock)) {
                auto &laser = debugLines.Get<ecs::LaserLine>(lock);
                if (!std::holds_alternative<ecs::LaserLine::Segments>(laser.line)) {
                    laser.line = ecs::LaserLine::Segments();
                }
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

        {
            ZoneScopedN("TransformCache Reset");
            // Reset dirty flags in transform cache outside of the transaction
            for (auto &[generation, cache] : transformCache) {
                if (generation != 0) cache.dirty = -1;
            }
        }
    }

    void PhysxManager::CreatePhysxScene() {
        ZoneScoped;
        PxSceneDesc sceneDesc(pxPhysics->getTolerancesScale());

        sceneDesc.gravity = PxVec3(0); // Gravity handled by scene properties
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;

        using Group = ecs::PhysicsGroup;
        // Don't collide the player with themselves, but allow the hands to collide with eachother
        PxSetGroupCollisionFlag((uint16_t)Group::Player, (uint16_t)Group::Player, false);
        PxSetGroupCollisionFlag((uint16_t)Group::Player, (uint16_t)Group::PlayerLeftHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::Player, (uint16_t)Group::PlayerRightHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::PlayerLeftHand, (uint16_t)Group::PlayerLeftHand, false);
        PxSetGroupCollisionFlag((uint16_t)Group::PlayerRightHand, (uint16_t)Group::PlayerRightHand, false);
        // Don't collide user interface elements with objects in the world or other interfaces
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::World, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::Interactive, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::HeldObject, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::Player, false);
        PxSetGroupCollisionFlag((uint16_t)Group::UserInterface, (uint16_t)Group::UserInterface, false);
        // Don't collide anything with the noclip group.
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::NoClip, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::World, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::Interactive, false);
        PxSetGroupCollisionFlag((uint16_t)Group::NoClip, (uint16_t)Group::HeldObject, false);
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

    size_t PhysxManager::UpdateShapes(const ecs::Physics &physics,
        const ecs::Entity &owner,
        const ecs::Entity &actorEnt,
        physx::PxRigidActor *actor,
        const ecs::Transform &offset) {
        // ZoneScoped;
        bool shapesChanged = false;
        std::vector<bool> existingShapes(physics.shapes.size());

        auto *userData = (ActorUserData *)actor->userData;
        if (!userData) return 0;

        size_t shapeCount = actor->getNbShapes();
        std::vector<PxShape *> pxShapes(shapeCount);
        actor->getShapes(pxShapes.data(), pxShapes.size());
        for (auto *pxShape : pxShapes) {
            auto shapeUserData = (ShapeUserData *)pxShape->userData;
            if (!shapeUserData || shapeUserData->owner != owner) continue;
            bool removeShape = false;
            if (shapeUserData->ownerShapeIndex >= existingShapes.size()) {
                removeShape = true;
            } else {
                auto &shape = physics.shapes[shapeUserData->ownerShapeIndex];
                if (shape.shape.index() != shapeUserData->shapeCache.shape.index()) {
                    removeShape = true;
                } else {
                    auto mesh = std::get_if<ecs::PhysicsShape::ConvexMesh>(&shape.shape);
                    if (mesh) {
                        auto meshSettings = mesh->hullSettings->Get();
                        auto sourceSettings = shapeUserData->hullCache ? shapeUserData->hullCache->sourceSettings->Get()
                                                                       : nullptr;
                        if (!meshSettings || !sourceSettings) {
                            removeShape = true;
                        } else if (mesh->model != shapeUserData->hullCache->sourceModel ||
                                   meshSettings->sourceInfo != sourceSettings->sourceInfo) {
                            removeShape = true;
                        } else {
                            existingShapes[shapeUserData->ownerShapeIndex] = true;

                            // Update matching mesh
                            // if (glm::any(glm::notEqual(shape.transform.matrix,
                            //         shapeUserData->shapeCache.transform.matrix,
                            //         1e-4f)) ||
                            //     glm::any(glm::notEqual(offset.matrix, shapeUserData->shapeOffset.matrix, 1e-4f))) {
                            //     // Logf("Updating actor mesh: %s", ecs::ToString(lock, e));
                            //     PxConvexMeshGeometry meshGeom;
                            //     if (pxShape->getConvexMeshGeometry(meshGeom)) {
                            //         meshGeom.scale = PxMeshScale(
                            //             GlmVec3ToPxVec3(glm::abs(offset * glm::vec4(shape.transform.GetScale(),
                            //             0))));
                            //         PxTransform shapeTransform(
                            //             GlmVec3ToPxVec3(offset * glm::vec4(shape.transform.GetPosition(), 1)),
                            //             GlmQuatToPxQuat(offset.GetRotation() * shape.transform.GetRotation()));
                            //         pxShape->setLocalPose(shapeTransform);
                            //         if (!meshGeom.isValid()) {
                            //             Errorf("Mesh geometry invalid: %s, %u", mesh->meshName, meshGeom.isValid());
                            //         }
                            //         meshGeom.convexMesh->acquireReference();
                            //         pxShape->setGeometry(meshGeom);
                            //         meshGeom.convexMesh->release();
                            //         shapeUserData->shapeCache = shape;
                            //         shapeUserData->shapeOffset = offset;
                            //     } else {
                            //         removeShape = true;
                            //     }
                            //     shapesChanged = true;
                            // }
                        }
                    } else if (existingShapes[shapeUserData->ownerShapeIndex]) {
                        removeShape = true;
                    } else {
                        existingShapes[shapeUserData->ownerShapeIndex] = true;

                        // Update matching shape
                        if (shape.shape != shapeUserData->shapeCache.shape) {
                            // Logf("Updating actor shape geometry: index %u", shapeIndex.second);
                            auto geometry = GeometryFromShape(shape, offset.GetScale());
                            pxShape->setGeometry(geometry.any());
                            shapeUserData->shapeCache.shape = shape.shape;
                            shapesChanged = true;
                        }
                        if (glm::any(glm::notEqual(shape.transform.matrix,
                                shapeUserData->shapeCache.transform.matrix,
                                1e-4f)) ||
                            glm::any(glm::notEqual(offset.matrix, shapeUserData->shapeOffset.matrix, 1e-4f))) {
                            // Logf("Updating actor shape pose: index %u", shapeIndex.second);

                            PxTransform shapeTransform(
                                GlmVec3ToPxVec3(offset * glm::vec4(shape.transform.GetPosition(), 1)),
                                GlmQuatToPxQuat(offset.GetRotation() * shape.transform.GetRotation()));
                            pxShape->setLocalPose(shapeTransform);
                            shapeUserData->shapeCache.transform = shape.transform;
                            shapeUserData->shapeOffset = offset;
                            shapesChanged = true;
                        }
                    }
                }
            }

            if (removeShape) {
                delete shapeUserData;
                pxShape->userData = nullptr;
                actor->detachShape(*pxShape);
                shapeCount--;
                shapesChanged = true;
            }
        }
        for (size_t i = 0; i < physics.shapes.size(); i++) {
            if (existingShapes[i]) continue;
            auto &shape = physics.shapes[i];
            // TODO: Add these material properties to the shape definition
            std::shared_ptr<PxMaterial> material(pxPhysics->createMaterial(0.6f, 0.5f, 0.0f), [](auto *ptr) {
                ptr->release();
            });
            auto mesh = std::get_if<ecs::PhysicsShape::ConvexMesh>(&shape.shape);
            if (mesh) {
                auto shapeCache = LoadConvexHullSet(mesh->model, mesh->hullSettings)->Get();

                if (shapeCache) {
                    for (auto &hull : shapeCache->hulls) {
                        auto *pxHull = hull.get();
                        if (mesh->hullSettings->Get()->name == "duck.cooked") {
                            Logf("Duck pxMesh ref count: %u", pxHull->getReferenceCount());
                        }
                        PxShape *pxShape = PxRigidActorExt::createExclusiveShape(*actor,
                            PxConvexMeshGeometry(pxHull,
                                PxMeshScale(GlmVec3ToPxVec3(shape.transform.GetScale() * offset.GetScale()))),
                            *material);
                        Assertf(pxShape, "Failed to create physx shape");
                        if (mesh->hullSettings->Get()->name == "duck.cooked") {
                            Logf("After Duck pxMesh ref count: %u", pxHull->getReferenceCount());
                        }

                        auto shapeUserData = new ShapeUserData(owner, i, actorEnt, material);
                        shapeUserData->shapeCache.shape = shape.shape;
                        shapeUserData->hullCache = shapeCache;
                        pxShape->userData = shapeUserData;

                        PxTransform shapeTransform(
                            GlmVec3ToPxVec3(offset * glm::vec4(shape.transform.GetPosition(), 1)),
                            GlmQuatToPxQuat(offset.GetRotation() * shape.transform.GetRotation()));
                        pxShape->setLocalPose(shapeTransform);

                        SetCollisionGroup(pxShape, userData->physicsGroup);
                        shapeCount++;
                    }
                } else {
                    Errorf("Physics actor created with invalid mesh: %s", mesh->meshName);
                }
            } else {
                PxShape *pxShape = PxRigidActorExt::createExclusiveShape(*actor,
                    GeometryFromShape(shape, offset.GetScale()).any(),
                    *material);
                Assertf(pxShape, "Failed to create physx shape");

                auto shapeUserData = new ShapeUserData(owner, i, actorEnt, material);
                shapeUserData->shapeCache.shape = shape.shape;
                pxShape->userData = shapeUserData;

                PxTransform shapeTransform(GlmVec3ToPxVec3(offset * glm::vec4(shape.transform.GetPosition(), 1)),
                    GlmQuatToPxQuat(offset.GetRotation() * shape.transform.GetRotation()));
                pxShape->setLocalPose(shapeTransform);

                SetCollisionGroup(pxShape, userData->physicsGroup);
                shapeCount++;
            }
        }

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic && shapesChanged) {
            if (physics.mass > 0.0f) {
                PxRigidBodyExt::setMassAndUpdateInertia(*dynamic, physics.mass);
            } else {
                PxRigidBodyExt::updateMassAndInertia(*dynamic, physics.density);
            }
        }
        return shapeCount;
    }

    PxRigidActor *PhysxManager::CreateActor(ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree, ecs::Physics>> lock,
        const ecs::Entity &e) {
        ZoneScoped;
        ZoneStr(ecs::ToString(lock, e));
        auto &ph = e.Get<ecs::Physics>(lock);

        auto &transform = e.Get<ecs::TransformTree>(lock);
        auto globalTransform = transform.GetGlobalTransform(lock);
        auto scale = globalTransform.GetScale();

        auto pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform.GetPosition()),
            GlmQuatToPxQuat(globalTransform.GetRotation()));

        PxRigidActor *actor = nullptr;
        if (ph.type == ecs::PhysicsActorType::Static) {
            actor = pxPhysics->createRigidStatic(pxTransform);
        } else if (ph.type == ecs::PhysicsActorType::Dynamic || ph.type == ecs::PhysicsActorType::Kinematic) {
            actor = pxPhysics->createRigidDynamic(pxTransform);

            if (ph.type == ecs::PhysicsActorType::Kinematic) {
                actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
                actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES,
                    true);
            }
        }
        Assert(actor, "Physx did not return valid PxRigidActor");

        actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);

        auto userData = new ActorUserData(e, globalTransform, ph.group);
        actor->userData = userData;

        ecs::Transform shapeOffset;
        shapeOffset.SetScale(scale);
        auto shapeCount = UpdateShapes(ph, e, e, actor, shapeOffset);

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic) {
            dynamic->setAngularDamping(ph.angularDamping);
            dynamic->setLinearDamping(ph.linearDamping);

            userData->angularDamping = ph.angularDamping;
            userData->linearDamping = ph.linearDamping;
        }

        if (shapeCount == 0) return actor;
        scene->addActor(*actor);
        return actor;
    }

    void PhysxManager::UpdateActor(
        ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree, ecs::Physics, ecs::SceneProperties>> lock,
        ecs::Entity &e) {
        // ZoneScoped;
        // ZoneStr(ecs::ToString(lock, e));
        auto &ph = e.Get<ecs::Physics>(lock);
        auto actorEnt = ph.parentActor.Get(lock);
        if (ph.type == ecs::PhysicsActorType::SubActor) {
            if (!actorEnt.Has<ecs::Physics, ecs::TransformTree>(lock)) {
                auto parentActor = e;
                while (parentActor.Has<ecs::TransformTree>(lock)) {
                    auto &tree = parentActor.Get<ecs::TransformTree>(lock);
                    parentActor = tree.parent.Get(lock);
                    if (parentActor.Has<ecs::Physics, ecs::TransformTree>(lock)) {
                        break;
                    }
                }
                if (parentActor.Has<ecs::Physics, ecs::TransformTree>(lock)) {
                    actorEnt = parentActor;
                } else {
                    return;
                }
            }
        }
        if (!actorEnt.Has<ecs::Physics, ecs::TransformTree>(lock)) {
            actorEnt = e;
        }
        if (actors.count(actorEnt) == 0) {
            if (actorEnt == e) {
                auto actor = CreateActor(lock, e);
                if (actor) actors[e] = actor;
            }
            return;
        }
        if (actorEnt != e && actors.count(e) != 0) {
            RemoveActor(actors[e]);
            actors.erase(e);
        }

        auto &actor = actors[actorEnt];
        auto dynamic = actor->is<PxRigidDynamic>();
        if (actorEnt == e) {
            bool requestDynamicActor = ph.type == ecs::PhysicsActorType::Dynamic ||
                                       ph.type == ecs::PhysicsActorType::Kinematic;
            if (requestDynamicActor != !!dynamic) {
                RemoveActor(actor);
                auto replacementActor = CreateActor(lock, e);
                if (replacementActor) {
                    actors[e] = replacementActor;
                } else {
                    actors.erase(e);
                }
                return;
            }
        }

        auto actorTransform = actorEnt.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
        auto subActorOffset = e.Get<ecs::TransformTree>(lock).GetRelativeTransform(lock, actorEnt);
        auto scale = actorTransform.GetScale();
        auto userData = (ActorUserData *)actor->userData;

        ecs::Transform shapeOffset = subActorOffset;
        shapeOffset.SetPosition(shapeOffset.GetPosition() * scale);
        shapeOffset.Scale(scale);
        auto shapeCount = UpdateShapes(ph, e, actorEnt, actor, shapeOffset);

        if (actorEnt == e) {
            if (glm::any(glm::notEqual(actorTransform.matrix, userData->pose.matrix, 1e-5f))) {
                // Logf("Updating actor position: %s", ecs::ToString(lock, e));
                PxTransform pxTransform(GlmVec3ToPxVec3(actorTransform.GetPosition()),
                    GlmQuatToPxQuat(actorTransform.GetRotation()));
                if (pxTransform.isSane()) {
                    if (dynamic && ph.type == ecs::PhysicsActorType::Kinematic) {
                        dynamic->setKinematicTarget(pxTransform);
                    } else {
                        actor->setGlobalPose(pxTransform);
                    }
                } else {
                    Errorf("Actor transform pose is not valid for entity: %s", ecs::ToString(lock, e));
                }

                userData->velocity = (actorTransform.GetPosition() - userData->pose.GetPosition()) *
                                     (float)(1e9 / interval.count());
            } else {
                userData->velocity = glm::vec3(0);
            }
            userData->pose = actorTransform;
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
                    auto &sceneProperties = ecs::SceneProperties::Get(lock, e);
                    glm::vec3 gravityForce = sceneProperties.GetGravity(actorTransform.GetPosition());
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

        if (!actor->getScene() && shapeCount > 0) {
            scene->addActor(*actor);
        }
    }

    void PhysxManager::RemoveActor(PxRigidActor *actor) {
        if (actor) {
            auto userData = (ActorUserData *)actor->userData;

            auto scene = actor->getScene();
            if (scene) scene->removeActor(*actor);
            PxU32 nShapes = actor->getNbShapes();
            vector<PxShape *> shapes(nShapes);
            actor->getShapes(shapes.data(), nShapes);
            Logf("Releasing actor: %s", ecs::EntityRef(userData->entity).Name().String());
            for (size_t i = 0; i < shapes.size(); i++) {
                auto *shape = shapes[i];
                auto shapeUserData = (ShapeUserData *)shape->userData;
                if (shapeUserData) {
                    delete shapeUserData;
                    shape->userData = nullptr;
                }
                actor->detachShape(*shape);
            }
            actor->release();

            if (userData) {
                delete userData;
                actor->userData = nullptr;
            }
        }
    }

    void PhysxManager::SetCollisionGroup(physx::PxRigidActor *actor, ecs::PhysicsGroup group) {
        PxU32 nShapes = actor->getNbShapes();
        vector<PxShape *> shapes(nShapes);
        actor->getShapes(shapes.data(), nShapes);

        for (uint32 i = 0; i < nShapes; ++i) {
            SetCollisionGroup(shapes[i], group);
        }

        auto userData = (ActorUserData *)actor->userData;
        if (userData) userData->physicsGroup = group;
    }

    void PhysxManager::SetCollisionGroup(PxShape *shape, ecs::PhysicsGroup group) {
        PxFilterData queryFilter, simulationFilter;
        queryFilter.word0 = 1 << (size_t)group;
        simulationFilter.word0 = (uint32_t)group;
        shape->setQueryFilterData(queryFilter);
        shape->setSimulationFilterData(simulationFilter);
    }

    PxGeometryHolder PhysxManager::GeometryFromShape(const ecs::PhysicsShape &shape, glm::vec3 parentScale) const {
        auto scale = shape.transform.GetScale() * parentScale;
        return std::visit(
            [&scale](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same<ecs::PhysicsShape::Sphere, T>()) {
                    auto avgScale = (scale.x + scale.y + scale.z) / 3.0f;
                    auto geom = PxSphereGeometry(avgScale * arg.radius);
                    Assertf(geom.isValid(), "Invalid shape geometry: Sphere");
                    return PxGeometryHolder(geom);
                } else if constexpr (std::is_same<ecs::PhysicsShape::Capsule, T>()) {
                    auto avgScaleYZ = (scale.y + scale.z) / 2.0f;
                    auto geom = PxCapsuleGeometry(avgScaleYZ * arg.radius, scale.x * arg.height * 0.5f);
                    Assertf(geom.isValid(), "Invalid shape geometry: Capsule");
                    return PxGeometryHolder(geom);
                } else if constexpr (std::is_same<ecs::PhysicsShape::Box, T>()) {
                    auto geom = PxBoxGeometry(GlmVec3ToPxVec3(scale * arg.extents * 0.5f));
                    Assertf(geom.isValid(), "Invalid shape geometry: Box");
                    return PxGeometryHolder(geom);
                } else if constexpr (std::is_same<ecs::PhysicsShape::Plane, T>()) {
                    auto geom = PxPlaneGeometry();
                    Assertf(geom.isValid(), "Invalid shape geometry: Plane");
                    return PxGeometryHolder(geom);
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
