#include "PhysxManager.hh"

#include "PhysxUtils.hh"
#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "core/CFunc.hh"
#include "core/CVar.hh"
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
    static CVar<float> CVarGravity("x.Gravity", -9.81f, "Acceleration due to gravity (m/sec^2)");
    static CVar<bool> CVarShowShapes("x.ShowShapes", false, "Show (1) or hide (0) the outline of physx collision shapes");
    static CVar<bool> CVarPropJumping("x.PropJumping", false, "Disable player collision with held object");
    static CVar<float> CVarMaxConstraintForce("x.MaxConstraintForce", 50.0f, "The maximum linear force for constraints");
    static CVar<float> CVarMaxConstraintTorque("x.MaxConstraintTorque", 20.0f, "The maximum torque force for constraints");
    // clang-format on

    PhysxManager::PhysxManager() : RegisteredThread("PhysX", 120.0), humanControlSystem(this) {
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

        if (CVarShowShapes.Changed()) { ToggleDebug(CVarShowShapes.Get(true)); }
        // if (CVarShowShapes.Get()) { CacheDebugLines(); }

        { // Sync ECS state to physx
            auto lock = ecs::World.StartTransaction<
                ecs::Write<ecs::Physics, ecs::Transform, ecs::HumanController, ecs::InteractController>>();

            // Delete actors for removed entities
            ecs::Removed<ecs::Physics> removedPhysics;
            while (physicsRemoval.Poll(lock, removedPhysics)) {
                if (removedPhysics.component.actor) { RemoveActor(removedPhysics.component.actor); }
            }
            ecs::Removed<ecs::HumanController> removedHumanController;
            while (humanControllerRemoval.Poll(lock, removedHumanController)) {
                if (removedHumanController.component.pxController) {
                    removedHumanController.component.pxController->release();
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

            // Update constraint forces
            for (auto constraint = constraints.begin(); constraint != constraints.end();) {
                auto &transform = constraint->parent.Get<ecs::Transform>(lock);
                auto pose = constraint->child->getGlobalPose();
                auto rotate = transform.GetRotate();
                auto invRotate = glm::inverse(rotate);

                auto targetPos = transform.GetPosition() + rotate * PxVec3ToGlmVec3P(constraint->offset);
                auto currentPos = pose.transform(constraint->child->getCMassLocalPose().transform(PxVec3(0.0)));
                auto deltaPos = GlmVec3ToPxVec3(targetPos) - currentPos;

                auto upAxis = GlmVec3ToPxVec3(invRotate * glm::vec3(0, 1, 0));
                constraint->rotationOffset = PxQuat(constraint->rotation.y, upAxis) * constraint->rotationOffset;
                constraint->rotationOffset = PxQuat(constraint->rotation.x, PxVec3(1, 0, 0)) *
                                             constraint->rotationOffset;

                auto targetRotate = rotate * PxQuatToGlmQuat(constraint->rotationOffset);
                auto currentRotate = PxQuatToGlmQuat(pose.q);
                auto deltaRotate = targetRotate * glm::inverse(currentRotate);

                if (glm::angle(deltaRotate) > M_PI_2) {
                    constraint->rotationOffset = GlmQuatToPxQuat(invRotate * currentRotate);
                }

                if (deltaPos.magnitude() < 2.0) {

                    /*{ // Apply Torque
                        auto deltaRotation = GlmVec3ToPxVec3(glm::eulerAngles(deltaRotate));

                        auto maxAcceleration = CVarMaxConstraintTorque.Get() / constraint->child->getMass();
                        auto deltaTick = maxAcceleration * (float)(this->interval.count() / 1e9);
                        auto maxVelocity = PxVec3(
                            std::max(0.0f, std::sqrtf(2.0f * maxAcceleration * std::abs(deltaRotation.x)) - deltaTick),
                            std::max(0.0f, std::sqrtf(2.0f * maxAcceleration * std::abs(deltaRotation.y)) - deltaTick),
                            std::max(0.0f, std::sqrtf(2.0f * maxAcceleration * std::abs(deltaRotation.z)) - deltaTick));

                        auto targetVelocity = maxVelocity.multiply(PxVec3(deltaRotation.x > 0 ? 1 : -1,
                                                                          deltaRotation.y > 0 ? 1 : -1,
                                                                          deltaRotation.z > 0 ? 1 : -1));
                        auto deltaVelocity = targetVelocity - constraint->child->getAngularVelocity();

                        auto force = deltaVelocity * (1e9 / this->interval.count() * constraint->child->getMass());
                        auto forceClampRatio = std::min(CVarMaxConstraintTorque.Get(), force.magnitude() + 0.00001f) /
                                               (force.magnitude() + 0.00001f);
                        constraint->child->addTorque(force * forceClampRatio);
                        constraint->rotation = PxVec3(0); // Don't continue to rotate
                    }*/

                    constraint->child->setAngularVelocity(GlmVec3ToPxVec3(glm::eulerAngles(deltaRotate)) *
                                                          (1e9 / this->interval.count()));
                    constraint->rotation = PxVec3(0); // Don't continue to rotate

                    { // Apply Force
                        auto maxAcceleration = PxVec3(CVarMaxConstraintForce.Get() / constraint->child->getMass());
                        if (deltaPos.y > 0) {
                            maxAcceleration.y -= CVarGravity.Get();
                        } else {
                            maxAcceleration.y += CVarGravity.Get();
                        }
                        maxAcceleration.y = std::max(0.0f, maxAcceleration.y);

                        auto deltaTick = maxAcceleration * (float)(this->interval.count() / 1e9);
                        auto maxVelocity = PxVec3(
                            std::max(0.0f, std::sqrtf(2.0f * maxAcceleration.x * std::abs(deltaPos.x)) - deltaTick.x),
                            std::max(0.0f, std::sqrtf(2.0f * maxAcceleration.y * std::abs(deltaPos.y)) - deltaTick.y),
                            std::max(0.0f, std::sqrtf(2.0f * maxAcceleration.z * std::abs(deltaPos.z)) - deltaTick.z));

                        auto targetVelocity = maxVelocity.multiply(
                            PxVec3(deltaPos.x > 0 ? 1 : -1, deltaPos.y > 0 ? 1 : -1, deltaPos.z > 0 ? 1 : -1));
                        auto deltaVelocity = targetVelocity - constraint->child->getLinearVelocity();

                        auto force = deltaVelocity * (1e9 / this->interval.count() * constraint->child->getMass());
                        force.y -= CVarGravity.Get() * constraint->child->getMass();
                        auto forceClampRatio = std::min(CVarMaxConstraintForce.Get(), force.magnitude() + 0.00001f) /
                                               (force.magnitude() + 0.00001f);
                        constraint->child->addForce(force * forceClampRatio);
                    }

                    /*{ // Apply Force (linear implementation)
                        auto maxAcceleration = CVarMaxConstraintForce.Get() / constraint->child->getMass();
                        if (deltaPos.y > 0) {
                            maxAcceleration -= CVarGravity.Get();
                        } else {
                            maxAcceleration += CVarGravity.Get();
                        }
                        auto deltaTick = maxAcceleration * (float)(this->interval.count() / 1e9);
                        auto maxVelocity = std::sqrtf(2.0f * std::max(0.0f, maxAcceleration) * std::abs(deltaPos.y));
                        maxVelocity = std::max(0.0f, maxVelocity - deltaTick);

                        auto targetVelocity = maxVelocity * (deltaPos.y > 0 ? 1 : -1);
                        auto deltaVelocity = targetVelocity - constraint->child->getLinearVelocity().y;

                        float force = deltaVelocity * (1e9 / this->interval.count() * constraint->child->getMass());
                        force -= CVarGravity.Get() * constraint->child->getMass();
                        auto forceClampRatio = std::min(CVarMaxConstraintForce.Get(), std::abs(force) + 0.00001f) /
                                               (std::abs(force) + 0.00001f);
                        constraint->child->addForce(PxVec3(0, force * forceClampRatio, 0));
                    }*/
                    constraint++;
                } else {
                    // Remove the constraint if the distance is too far
                    if (constraint->parent.Has<ecs::InteractController>(lock)) {
                        constraint->parent.Get<ecs::InteractController>(lock).target = nullptr;
                    }

                    PxU32 nShapes = constraint->child->getNbShapes();
                    vector<PxShape *> shapes(nShapes);
                    constraint->child->getShapes(shapes.data(), nShapes);

                    PxFilterData data;
                    data.word0 = PhysxCollisionGroup::WORLD;
                    for (uint32 i = 0; i < nShapes; ++i) {
                        shapes[i]->setQueryFilterData(data);
                        shapes[i]->setSimulationFilterData(data);
                    }
                    constraint = constraints.erase(constraint);
                }
            }
        }

        humanControlSystem.Frame(std::chrono::nanoseconds(this->interval).count() / 1e9);

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
                auto &transform = ent.Get<ecs::Transform>(lock);

                if (!ph.dynamic) continue;

                Assert(!transform.HasParent(lock), "Dynamic physics objects must have no parent");

                if (ph.actor && !transform.IsDirty()) {
                    auto pose = ph.actor->getGlobalPose();
                    transform.SetPosition(PxVec3ToGlmVec3P(pose.p), false);
                    transform.SetRotate(PxQuatToGlmQuat(pose.q), false);

                    transform.UpdateCachedTransform(lock);
                }
            }
        }
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
            physicsRemoval = lock.Watch<ecs::Removed<ecs::Physics>>();
            humanControllerRemoval = lock.Watch<ecs::Removed<ecs::HumanController>>();
        }
    }

    void PhysxManager::ToggleDebug(bool enabled) {
        debug = enabled;
        float scale = (enabled ? 1.0f : 0.0f);

        scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, scale);
        scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, scale);
    }

    bool PhysxManager::IsDebugEnabled() const {
        return debug;
    }

    // void PhysxManager::CacheDebugLines() {
    //     const PxRenderBuffer &rb = scene->getRenderBuffer();
    //     const PxDebugLine *lines = rb.getLines();

    //     {
    //         std::lock_guard<std::mutex> lock(debugLinesMutex);
    //         debugLines = vector<PxDebugLine>(lines, lines + rb.getNbLines());
    //     }
    // }

    // MutexedVector<PxDebugLine> PhysxManager::GetDebugLines() {
    //     return MutexedVector<PxDebugLine>(debugLines, debugLinesMutex);
    // }

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
        if (!flags.isSet(PxRigidBodyFlag::eKINEMATIC)) {
            throw std::runtime_error("cannot translate a non-kinematic actor");
        }

        PxTransform pose = actor->getGlobalPose();
        pose.p += transform;
        actor->setKinematicTarget(pose);
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

    void PhysxManager::UpdateActor(ecs::Lock<ecs::Write<ecs::Physics, ecs::Transform>> lock, Tecs::Entity &e) {
        auto &ph = e.Get<ecs::Physics>(lock);
        auto &transform = e.Get<ecs::Transform>(lock);

        if (!ph.model || !ph.model->Valid()) return;

        auto globalTransform = transform.GetGlobalTransform(lock);
        auto globalRotation = transform.GetGlobalRotation(lock);
        auto scale = glm::vec3(glm::inverse(globalRotation) * (globalTransform * glm::vec4(1, 1, 1, 0)));

        auto pxTransform = PxTransform(GlmVec3ToPxVec3(globalTransform * glm::vec4(0, 0, 0, 1)),
                                       GlmQuatToPxQuat(globalRotation));

        if (!ph.actor) {
            if (ph.dynamic) {
                Assert(!transform.HasParent(lock), "Dynamic physics objects must have no parent");

                ph.actor = pxPhysics->createRigidDynamic(pxTransform);

                if (ph.kinematic) { ph.actor->is<PxRigidBody>()->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true); }
            } else {
                ph.actor = pxPhysics->createRigidStatic(pxTransform);
            }
            Assert(ph.actor, "Physx did not return valid PxRigidActor");

            ph.actor->userData = reinterpret_cast<void *>((size_t)e.id);

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

                auto shape = PxRigidActorExt::createExclusiveShape(
                    *ph.actor,
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

        if (transform.IsDirty()) {
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
            transform.ClearDirty();
        }
    }

    void PhysxManager::RemoveActor(PxRigidActor *actor) {
        if (actor) {
            auto rigidBody = actor->is<PxRigidDynamic>();
            if (rigidBody) RemoveConstraints(rigidBody);

            scene->removeActor(*actor);
            actor->release();
        }
    }

    void ControllerHitReport::onShapeHit(const PxControllerShapeHit &hit) {
        auto dynamic = hit.actor->is<PxRigidDynamic>();
        if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) {
            glm::vec3 *velocity = (glm::vec3 *)hit.controller->getUserData();
            auto magnitude = glm::length(*velocity);
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

    void PhysxManager::UpdateController(ecs::Lock<ecs::Write<ecs::HumanController, ecs::Transform>> lock,
                                        Tecs::Entity &e) {

        auto &controller = e.Get<ecs::HumanController>(lock);
        auto &transform = e.Get<ecs::Transform>(lock);

        auto position = transform.GetGlobalPosition(lock);
        // Offset the capsule position so the camera (transform origin) is at the top
        auto capsuleHeight = controller.pxController ? controller.pxController->getHeight() : controller.height;
        auto pxPosition = GlmVec3ToPxExtendedVec3(position - glm::vec3(0, capsuleHeight / 2, 0));

        if (!controller.pxController) {
            // Capsule controller description will want to be data driven
            PxCapsuleControllerDesc desc;
            desc.position = pxPosition;
            desc.upDirection = PxVec3(0, 1, 0);
            desc.radius = ecs::PLAYER_RADIUS;
            desc.height = controller.height;
            desc.density = 0.5f;
            desc.material = pxPhysics->createMaterial(0.3f, 0.3f, 0.3f);
            desc.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
            desc.reportCallback = new ControllerHitReport(); // TODO: Does this have to be free'd?
            desc.userData = new glm::vec3(0); // TODO: Store this on the component

            auto pxController = controllerManager->createController(desc);
            Assert(pxController->getType() == PxControllerShapeType::eCAPSULE,
                   "Physx did not create a valid PxCapsuleController");

            pxController->setStepOffset(ecs::PLAYER_STEP_HEIGHT);

            PxShape *shape;
            pxController->getActor()->getShapes(&shape, 1);
            PxFilterData data;
            data.word0 = PhysxCollisionGroup::PLAYER;
            shape->setQueryFilterData(data);
            shape->setSimulationFilterData(data);

            controller.pxController = static_cast<PxCapsuleController *>(pxController);
        }

        if (transform.IsDirty()) {
            controller.pxController->setPosition(pxPosition);
            transform.ClearDirty();
        }

        float currentHeight = controller.pxController->getHeight();
        if (currentHeight != controller.height) {
            auto currentPos = controller.pxController->getFootPosition();
            controller.pxController->resize(controller.height);
            if (!controller.onGround) {
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

    bool PhysxManager::SweepQuery(PxRigidDynamic *actor, const PxVec3 dir, const float distance) {
        PxShape *shape;
        actor->getShapes(&shape, 1);

        PxCapsuleGeometry capsuleGeometry;
        shape->getCapsuleGeometry(capsuleGeometry);

        scene->removeActor(*actor);
        PxSweepBuffer hit;
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
        PxQueryFilterData filterData = PxQueryFilterData(PxQueryFlag::eANY_HIT | PxQueryFlag::eSTATIC |
                                                         PxQueryFlag::eDYNAMIC);
        PxTransform pose = actor->getGlobalPose();
        pose.p += translation;
        bool overlapFound = scene->overlap(capsuleGeometry, pose, hit, filterData);
        scene->addActor(*actor);

        return overlapFound;
    }

    void PhysxManager::CreateConstraint(ecs::Lock<> lock,
                                        Tecs::Entity parent,
                                        PxRigidDynamic *child,
                                        PxVec3 offset,
                                        PxQuat rotationOffset) {
        PxU32 nShapes = child->getNbShapes();
        vector<PxShape *> shapes(nShapes);
        child->getShapes(shapes.data(), nShapes);

        PxFilterData data;
        data.word0 = PhysxCollisionGroup::HELD_OBJECT;
        for (uint32 i = 0; i < nShapes; ++i) {
            shapes[i]->setQueryFilterData(data);
            shapes[i]->setSimulationFilterData(data);
        }

        PhysxConstraint constraint;
        constraint.parent = parent;
        constraint.child = child;
        constraint.offset = offset;
        constraint.rotationOffset = rotationOffset;
        constraint.rotation = PxVec3(0);

        if (parent.Has<ecs::Transform>(lock)) { constraints.emplace_back(constraint); }
    }

    void PhysxManager::RotateConstraint(Tecs::Entity parent, PxRigidDynamic *child, PxVec3 rotation) {
        for (auto it = constraints.begin(); it != constraints.end();) {
            if (it->parent == parent && it->child == child) {
                it->rotation = rotation;
                break;
            }
        }
    }

    void PhysxManager::RemoveConstraint(Tecs::Entity parent, PxRigidDynamic *child) {
        PxU32 nShapes = child->getNbShapes();
        vector<PxShape *> shapes(nShapes);
        child->getShapes(shapes.data(), nShapes);

        PxFilterData data;
        data.word0 = PhysxCollisionGroup::WORLD;
        for (uint32 i = 0; i < nShapes; ++i) {
            shapes[i]->setQueryFilterData(data);
            shapes[i]->setSimulationFilterData(data);
        }

        for (auto it = constraints.begin(); it != constraints.end();) {
            if (it->parent == parent && it->child == child) {
                it = constraints.erase(it);
            } else
                it++;
        }
    }

    void PhysxManager::RemoveConstraints(PxRigidDynamic *child) {
        PxU32 nShapes = child->getNbShapes();
        vector<PxShape *> shapes(nShapes);
        child->getShapes(shapes.data(), nShapes);

        PxFilterData data;
        data.word0 = PhysxCollisionGroup::WORLD;
        for (uint32 i = 0; i < nShapes; ++i) {
            shapes[i]->setQueryFilterData(data);
            shapes[i]->setSimulationFilterData(data);
        }

        for (auto it = constraints.begin(); it != constraints.end();) {
            if (it->child == child) {
                it = constraints.erase(it);
            } else
                it++;
        }

        // TODO: Also remove contraints with this parent
    }

    // Increment if the Collision Cache format ever changes
    const uint32 hullCacheMagic = 0xc042;

    ConvexHullSet *PhysxManager::LoadCollisionCache(const Model &model, bool decomposeHull) {
        std::ifstream in;

        std::string path = "cache/collision/" + model.name;
        if (decomposeHull) path += "-decompose";

        if (GAssets.InputStream(path, in)) {
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
