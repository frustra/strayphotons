/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "PhysxManager.hh"

#include "PhysxUtils.hh"
#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "assets/JsonHelpers.hh"
#include "assets/PhysicsInfo.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "common/Tracing.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
#include "game/GameLogic.hh"
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

    PhysxManager::PhysxManager(LockFreeEventQueue<ecs::Event> &windowInputQueue)
        : RegisteredThread("PhysX", 120.0, true), windowInputQueue(windowInputQueue), characterControlSystem(*this),
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
        funcs.Register<unsigned int>("stepphysics",
            "Advance the physics simulation by N frames, default is 1",
            [this](unsigned int arg) {
                this->Step(std::max(1u, arg));
            });
        funcs.Register("pausephysics", "Pause the physics simulation (See also: resumephysics)", [this] {
            this->Pause(true);
        });
        funcs.Register("resumephysics", "Pause the physics simulation (See also: pausephysics)", [this] {
            this->Pause(false);
        });

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
        subActors.clear();
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

    void PhysxManager::StartThread(bool startPaused) {
        RegisteredThread::StartThread(startPaused);
    }

    void PhysxManager::PreFrame() {
        ZoneScoped;
        GetSceneManager().PreloadScenePhysics([this](auto lock, auto scene) {
            ZoneScopedN("PreloadScenePhysics");
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
        if (CVarPhysxDebugCollision.Changed()) {
            bool collision = CVarPhysxDebugCollision.Get(true);
            scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, collision ? 1 : 0);
            scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, collision);
        }

        characterControlSystem.RegisterEvents();

        { // Sync ECS state to physx
            ZoneScopedN("Sync ECS");
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock,
                ecs::Read<ecs::LaserEmitter,
                    ecs::LightSensor,
                    ecs::EventBindings,
                    ecs::Physics,
                    ecs::EventInput,
                    ecs::TriggerGroup,
                    ecs::CharacterController,
                    ecs::SceneProperties,
                    ecs::Scripts>,
                ecs::Write<ecs::Animation,
                    ecs::TransformSnapshot,
                    ecs::TransformTree,
                    ecs::TriggerArea,
                    ecs::PhysicsJoints,
                    ecs::CharacterController,
                    ecs::OpticalElement,
                    ecs::PhysicsQuery,
                    ecs::LaserLine,
                    ecs::LaserSensor,
                    ecs::Signals>,
                ecs::PhysicsUpdateLock>();

            GameLogic::UpdateInputEvents(lock, windowInputQueue);

            characterControlSystem.Frame(lock);

            {
                ZoneScopedN("UpdateSnapshots(Dynamic)");
                for (auto ent : lock.EntitiesWith<ecs::Physics>()) {
                    if (!ent.Has<ecs::Physics, ecs::TransformSnapshot, ecs::TransformTree>(lock)) continue;

                    auto &ph = ent.Get<ecs::Physics>(lock);
                    if (actors.count(ent) > 0) {
                        auto const &actor = actors[ent];
                        auto &transform = ent.Get<ecs::TransformSnapshot>(lock).globalPose;

                        auto userData = (ActorUserData *)actor->userData;
                        Assert(userData, "Physics actor is missing UserData");
                        if (ph.type == ecs::PhysicsActorType::Dynamic && transform == userData->pose) {
                            // Only update the ECS position if nothing has moved it during the PhysX simulation
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

                    triggerSystem.UpdateEntityTriggers(lock, ent);

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

            animationSystem.Frame(lock);

            // Delete actors for removed entities
            ecs::ComponentEvent<ecs::Physics> physicsEvent;
            while (physicsObserver.Poll(lock, physicsEvent)) {
                if (physicsEvent.type == Tecs::EventType::REMOVED) {
                    if (actors.count(physicsEvent.entity) > 0) {
                        RemoveActor(actors[physicsEvent.entity]);
                    } else if (subActors.count(physicsEvent.entity) > 0) {
                        RemoveActor(subActors[physicsEvent.entity]);
                    }
                }
            }

            {
                ZoneScopedN("UpdateActors");
                // Update actors with latest entity data
                for (auto &ent : lock.EntitiesWith<ecs::Physics>()) {
                    if (!ent.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
                    auto &ph = ent.Get<ecs::Physics>(lock);
                    if (ph.type == ecs::PhysicsActorType::SubActor) continue;
                    UpdateActor(lock, ent);
                }
            }

            {
                ZoneScopedN("UpdateSubActors");
                // Update sub actors once all parent actors are complete
                for (auto &ent : lock.EntitiesWith<ecs::Physics>()) {
                    if (!ent.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
                    auto &ph = ent.Get<ecs::Physics>(lock);
                    if (ph.type != ecs::PhysicsActorType::SubActor) continue;
                    UpdateActor(lock, ent);
                }
            }

            constraintSystem.Frame(lock);
            triggerSystem.Frame(lock);
            physicsQuerySystem.Frame(lock);
            laserSystem.Frame(lock);
            UpdateDebugLines(lock);

            ecs::GetScriptManager().RunOnPhysicsUpdate(lock, interval);
        }

        { // Simulate 1 physics frame (blocking)
            ZoneScopedN("Simulate");
            scene->simulate(PxReal(std::chrono::nanoseconds(this->interval).count() / 1e9),
                nullptr,
                scratchBlock.data(),
                scratchBlock.size());
            scene->fetchResults(true);
        }

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
        sceneDesc.filterShader = SimulationCallbackHandler::SimulationFilterShader;
        sceneDesc.simulationEventCallback = &simulationCallback;

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

        // TODO: Configure PhysX with more threads in the future
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

    size_t PhysxManager::UpdateShapes(ecs::Lock<ecs::Read<ecs::Name, ecs::Physics>> lock,
        const ecs::Entity &owner,
        const ecs::Entity &actorEnt,
        physx::PxRigidActor *actor,
        const ecs::Transform &offset) {
        // ZoneScoped;
        bool shapesChanged = false;
        auto &physics = owner.Get<ecs::Physics>(lock);
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
                    auto shapeTransform = offset * shape.transform;
                    bool transformMoved = glm::any(
                        glm::notEqual(shapeTransform.offset, shapeUserData->shapeTransform.offset, 1e-4f));
                    bool transformScaled = glm::any(
                        glm::notEqual(shapeTransform.scale, shapeUserData->shapeTransform.scale, 1e-4f));
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
                            PxConvexMeshGeometry meshGeom;
                            if (pxShape->getConvexMeshGeometry(meshGeom)) {
                                if (transformScaled) {
                                    // Logf("Updating actor mesh geometry: %s index %u",
                                    //     ecs::ToString(lock, owner),
                                    //     shapeUserData->ownerShapeIndex);

                                    meshGeom.scale = PxMeshScale(GlmVec3ToPxVec3(shapeTransform.GetScale()));
                                    Assertf(meshGeom.isValid(), "Invalid mesh geometry: %s", mesh->meshName);
                                    pxShape->setGeometry(meshGeom);

                                    shapeUserData->shapeCache = shape;
                                    shapeUserData->shapeTransform.scale = shapeTransform.scale;
                                    shapesChanged = true;
                                }
                                if (transformMoved) {
                                    // Logf("Updating actor mesh pose: %s index %u",
                                    //     ecs::ToString(lock, owner),
                                    //     shapeUserData->ownerShapeIndex);

                                    PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                                        GlmQuatToPxQuat(shapeTransform.GetRotation()));
                                    pxShape->setLocalPose(pxTransform);

                                    shapeUserData->shapeTransform.offset = shapeTransform.offset;
                                    shapesChanged = true;
                                }
                            } else {
                                removeShape = true;
                            }
                        }
                    } else if (existingShapes[shapeUserData->ownerShapeIndex]) {
                        removeShape = true;
                    } else {
                        existingShapes[shapeUserData->ownerShapeIndex] = true;

                        // Update matching shape
                        if (shape.shape != shapeUserData->shapeCache.shape || transformScaled) {
                            // Logf("Updating actor shape geometry: %s index %u",
                            //     ecs::ToString(lock, owner),
                            //     shapeUserData->ownerShapeIndex);

                            auto geometry = GeometryFromShape(shape, shapeTransform.GetScale());
                            pxShape->setGeometry(geometry.any());

                            shapeUserData->shapeCache = shape;
                            shapeUserData->shapeTransform.scale = shapeTransform.scale;
                            shapesChanged = true;
                        }
                        if (transformMoved) {
                            // Logf("Updating actor shape pose: %s index %u",
                            //     ecs::ToString(lock, owner),
                            //     shapeUserData->ownerShapeIndex);

                            PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                                GlmQuatToPxQuat(shapeTransform.GetRotation()));
                            pxShape->setLocalPose(pxTransform);

                            shapeUserData->shapeTransform.offset = shapeTransform.offset;
                            shapesChanged = true;
                        }
                    }
                }
            }

            if (removeShape) {
                actor->detachShape(*pxShape);
                // User data holds a reference to underlying shape memory
                // and must be destroyed after the shape is no longer active
                delete shapeUserData;
                shapeUserData = nullptr;

                shapeCount--;
                shapesChanged = true;
            }
        }
        for (size_t i = 0; i < physics.shapes.size(); i++) {
            if (existingShapes[i]) continue;
            auto &shape = physics.shapes[i];
            PxMaterial *pxMaterial = pxPhysics->createMaterial(shape.material.staticFriction,
                shape.material.dynamicFriction,
                shape.material.restitution);
            std::shared_ptr<PxMaterial> material(pxMaterial, [](auto *ptr) {
                ptr->release();
            });
            auto mesh = std::get_if<ecs::PhysicsShape::ConvexMesh>(&shape.shape);
            if (mesh) {
                auto shapeCache = LoadConvexHullSet(mesh->model, mesh->hullSettings)->Get();

                if (shapeCache) {
                    auto shapeTransform = offset * shape.transform;
                    for (auto &hull : shapeCache->hulls) {
                        PxMeshScale meshScale = GlmVec3ToPxVec3(shapeTransform.GetScale());
                        PxShape *pxShape = PxRigidActorExt::createExclusiveShape(*actor,
                            PxConvexMeshGeometry(hull.get(), meshScale),
                            *material);
                        Assertf(pxShape, "Failed to create physx shape");

                        PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                            GlmQuatToPxQuat(shapeTransform.GetRotation()));
                        pxShape->setLocalPose(pxTransform);

                        SetCollisionGroup(pxShape, userData->physicsGroup);

                        auto shapeUserData = new ShapeUserData(owner, i, actorEnt, material);
                        shapeUserData->shapeCache = shape;
                        shapeUserData->shapeTransform = shapeTransform;
                        shapeUserData->hullCache = shapeCache;
                        pxShape->userData = shapeUserData;

                        shapeCount++;
                        shapesChanged = true;
                    }
                } else {
                    Errorf("Physics actor created with invalid mesh: %s", mesh->meshName);
                }
            } else {
                auto shapeTransform = offset * shape.transform;
                PxShape *pxShape = PxRigidActorExt::createExclusiveShape(*actor,
                    GeometryFromShape(shape, shapeTransform.GetScale()).any(),
                    *material);
                Assertf(pxShape, "Failed to create physx shape");

                PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                    GlmQuatToPxQuat(shapeTransform.GetRotation()));
                pxShape->setLocalPose(pxTransform);

                SetCollisionGroup(pxShape, userData->physicsGroup);

                auto shapeUserData = new ShapeUserData(owner, i, actorEnt, material);
                shapeUserData->shapeCache = shape;
                shapeUserData->shapeTransform = shapeTransform;
                pxShape->userData = shapeUserData;

                shapeCount++;
                shapesChanged = true;
            }
        }

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic && shapesChanged) {
            Tracef("Updating actor inertia: %s", ecs::ToString(lock, actorEnt));
            auto &ph = actorEnt.Get<ecs::Physics>(lock);
            if (ph.mass > 0.0f) {
                PxRigidBodyExt::setMassAndUpdateInertia(*dynamic, ph.mass);
            } else {
                PxRigidBodyExt::updateMassAndInertia(*dynamic, ph.density);
            }
        }
        return shapeCount;
    }

    PxRigidActor *PhysxManager::CreateActor(ecs::Lock<ecs::Read<ecs::Name, ecs::TransformSnapshot, ecs::Physics>> lock,
        const ecs::Entity &e) {
        ZoneScoped;
        ZoneStr(ecs::ToString(lock, e));
        auto &ph = e.Get<ecs::Physics>(lock);

        auto &globalTransform = e.Get<ecs::TransformSnapshot>(lock).globalPose;
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
        auto shapeCount = UpdateShapes(lock, e, e, actor, shapeOffset);

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic) {
            dynamic->setAngularDamping(ph.angularDamping);
            dynamic->setLinearDamping(ph.linearDamping);

            userData->angularDamping = ph.angularDamping;
            userData->linearDamping = ph.linearDamping;
        }

        actors[e] = actor;
        if (shapeCount == 0) return actor;
        scene->addActor(*actor);
        return actor;
    }

    void PhysxManager::UpdateActor(
        ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree, ecs::TransformSnapshot, ecs::Physics, ecs::SceneProperties>>
            lock,
        const ecs::Entity &e) {
        ZoneScoped;
        auto &ph = e.Get<ecs::Physics>(lock);
        auto actorEnt = ph.parentActor.Get(lock);
        if (ph.type == ecs::PhysicsActorType::SubActor) {
            // ZoneStr(ecs::ToString(lock, e) + "/" + ecs::ToString(lock, actorEnt));
            if (!actorEnt.Has<ecs::Physics, ecs::TransformTree, ecs::TransformSnapshot>(lock)) {
                auto parentActor = e;
                while (parentActor.Has<ecs::TransformTree>(lock)) {
                    auto &tree = parentActor.Get<ecs::TransformTree>(lock);
                    parentActor = tree.parent.Get(lock);
                    if (parentActor.Has<ecs::Physics, ecs::TransformTree, ecs::TransformSnapshot>(lock)) {
                        break;
                    }
                }
                if (parentActor.Has<ecs::Physics, ecs::TransformTree, ecs::TransformSnapshot>(lock)) {
                    actorEnt = parentActor;
                } else {
                    return;
                }
            }
        } else {
            // ZoneStr(ecs::ToString(lock, e));
        }
        if (!actorEnt.Has<ecs::Physics, ecs::TransformTree, ecs::TransformSnapshot>(lock)) {
            actorEnt = e;
        }
        if (actors.count(actorEnt) == 0) {
            if (actorEnt == e) CreateActor(lock, e);
            return;
        }
        auto &actor = actors[actorEnt];
        if (actorEnt != e) {
            if (actors.count(e) > 0) RemoveActor(actors[e]);
            if (subActors.count(e) > 0) {
                if (subActors[e] != actor) {
                    RemoveActor(subActors[e]);
                    subActors[e] = actor;
                }
            } else {
                subActors[e] = actor;
            }
        }

        auto dynamic = actor->is<PxRigidDynamic>();
        if (actorEnt == e) {
            bool requestDynamicActor = ph.type == ecs::PhysicsActorType::Dynamic ||
                                       ph.type == ecs::PhysicsActorType::Kinematic;
            if (requestDynamicActor != !!dynamic) {
                RemoveActor(actor);
                CreateActor(lock, e);
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
        auto shapeCount = UpdateShapes(lock, e, actorEnt, actor, shapeOffset);

        if (actorEnt == e) {
            if (glm::any(glm::notEqual(actorTransform.offset, userData->pose.offset, 1e-5f))) {
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
            } else if (ph.type != ecs::PhysicsActorType::Dynamic) {
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
                if (userData->contactReportThreshold != ph.contactReportThreshold) {
                    dynamic->setContactReportThreshold(
                        ph.contactReportThreshold >= 0 ? ph.contactReportThreshold : PX_MAX_F32);
                    userData->contactReportThreshold = ph.contactReportThreshold;
                }
            }
        }

        if (!actor->getScene() && shapeCount > 0) {
            scene->addActor(*actor);
        }

        if (actor->getScene()) {
            auto globalPose = actor->getGlobalPose();
            if (actorEnt == e && dynamic) {
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
    }

    void PhysxManager::RemoveActor(PxRigidActor *actor) {
        ZoneScoped;
        if (actor) {
            auto userData = (ActorUserData *)actor->userData;
            if (userData) ZoneStr(std::to_string(userData->entity));

            auto scene = actor->getScene();
            if (scene) scene->removeActor(*actor);
            PxU32 nShapes = actor->getNbShapes();
            vector<PxShape *> shapes(nShapes);
            actor->getShapes(shapes.data(), nShapes);
            for (auto *shape : shapes) {
                auto shapeUserData = (ShapeUserData *)shape->userData;
                actor->detachShape(*shape);

                if (shapeUserData) {
                    // User data holds a reference to underlying shape memory
                    // and must be destroyed after the shape is no longer active
                    delete shapeUserData;
                }
            }
            actor->release();

            if (userData) delete userData;

            // Remove matching actors from the lookup maps
            actors.erase(actor);
            subActors.erase(actor);
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

    void PhysxManager::UpdateDebugLines(ecs::Lock<ecs::Write<ecs::LaserLine>> lock) const {
        auto debugLines = debugLineEntity.Get(lock);
        if (debugLines.Has<ecs::LaserLine>(lock)) {
            auto &laser = debugLines.Get<ecs::LaserLine>(lock);
            if (!std::holds_alternative<ecs::LaserLine::Segments>(laser.line)) {
                laser.line = ecs::LaserLine::Segments();
            }
            auto &segments = std::get<ecs::LaserLine::Segments>(laser.line);
            segments.clear();
            if (CVarPhysxDebugCollision.Get()) {
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
} // namespace sp
