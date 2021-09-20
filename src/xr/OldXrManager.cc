#ifdef SP_XR_SUPPORT

    #include "XrManager.hh"
    #include "assets/AssetManager.hh"
    #include "assets/Model.hh"
    #include "core/CVar.hh"
    #include "core/Console.hh"
    #include "core/Logging.hh"
    #include "ecs/Ecs.hh"
    #include "ecs/EcsImpl.hh"
    #include "game/Game.hh"
    #include "graphics/opengl/GLBuffer.hh"
    #include "graphics/opengl/GLModel.hh"
    #include "graphics/opengl/VertexBuffer.hh"
    #include "xr/XrSystemFactory.hh"

    #include <glm/glm.hpp>
    #include <glm/gtx/quaternion.hpp>
    #include <glm/gtx/transform.hpp>

namespace sp::xr {
    static CVar<bool> CVarConnectXR("xr.Connect", true, "Connect to a supported XR Runtime");
    static CVar<bool> CVarController("xr.Controllers", true, "Render controller models (if available)");

    enum SkeletonMode { SkeletonDisabled = 0, SkeletonNormal = 1, SkeletonDebug = 2 };
    static CVar<int> CVarSkeletons("xr.Skeletons", 1, "XR Skeleton mode (0: none, 1: normal, 2: debug)");

    class BasicModel : public Model {
    public:
        BasicModel(const string &name) : Model(name){};

        std::map<string, BasicMaterial> basicMaterials;
        std::map<string, VertexBuffer> vbos;
        std::map<string, GLBuffer> ibos;
    };

    XrManager::XrManager(Game *game) : game(game) {
        funcs.Register(this,
                       "setvrorigin",
                       "Move the VR origin to the current player position",
                       &XrManager::SetVrOrigin);
    }

    XrManager::~XrManager() {}

    bool XrManager::Frame(double dtSinceLastFrame) {
        // Handle xr controller movement
        if (xrSystem) {
            ecs::Entity vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");

            if (vrOrigin) {
                // TODO: support other action sets
                gameActionSet->Sync();

                // Mapping of Pose Actions to Subpaths. Needed so we can tell which-hand-did-what for the hand pose
                // linked actions
                vector<std::pair<xr::XrActionPtr, string>> controllerPoseActions = {
                    {leftHandAction, xr::SubpathLeftHand},
                    {rightHandAction, xr::SubpathRightHand}};

                for (auto controllerAction : controllerPoseActions) {
                    glm::mat4 xrObjectPos;
                    bool active = controllerAction.first->GetPoseActionValueForNextFrame(controllerAction.second,
                                                                                         xrObjectPos);
                    ecs::Entity xrObject = UpdateXrActionEntity(controllerAction.first, active && CVarController.Get());

                    if (xrObject.Valid()) {
                        ecs::Transform ctrl;
                        {
                            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::Transform>>();

                            auto &vrOriginTransformTecs = vrOrigin.GetEntity().Get<ecs::Transform>(lock);
                            xrObjectPos = glm::transpose(
                                xrObjectPos * glm::transpose(vrOriginTransformTecs.GetGlobalTransform(lock)));

                            auto &ctrlTransform = xrObject.GetEntity().Get<ecs::Transform>(lock);
                            ctrlTransform.SetPosition(xrObjectPos * glm::vec4(0, 0, 0, 1));
                            ctrlTransform.SetRotate(glm::mat4(glm::mat3(xrObjectPos)));
                            ctrl = ctrlTransform;
                        }

                        if (controllerAction.first == rightHandAction) {
                            // TODO: make this bound to the "dominant user hand", or pick the last hand the user tried
                            // to teleport with
                            // TODO: make this support "skeleton-only" mode
                            // FIXME: the laser pointer is now affected by shadows and is no longer emissive. #40
                            // Parent the laser pointer to the xrObject representing the last teleport action
                            ecs::Entity laserPointer = GetLaserPointer();

                            if (laserPointer.Valid()) {
                                auto transform = laserPointer.Get<ecs::Transform>();
                                auto parent = xrObject;

                                transform->SetPosition(glm::vec3(0, 0, 0));
                                transform->SetRotate(glm::quat());
                                transform->SetParent(parent.GetId());
                            }
                        }

    #ifdef SP_PHYSICS_SUPPORT_PHYSX
                        bool teleport = false;
                        teleportAction->GetRisingEdgeActionValue(controllerAction.second, teleport);

                        if (teleport) {
                            Logf("Teleport on subpath %s", controllerAction.second);

                            auto origin = ctrl.GetPosition();
                            auto dir = glm::normalize(ctrl.GetForward());
                            physx::PxReal maxDistance = 10.0f;

                            {
                                auto lock =
                                    ecs::World.StartTransaction<ecs::Read<ecs::HumanController>,
                                                                ecs::Write<ecs::PhysicsState, ecs::Transform>>();

                                physx::PxRaycastBuffer hit;
                                bool status = game->physics.RaycastQuery(lock,
                                                                         xrObject.GetEntity(),
                                                                         origin,
                                                                         dir,
                                                                         maxDistance,
                                                                         hit);

                                if (status && hit.block.distance > 0.5) {

                                    auto &vrOriginTransform = vrOrigin.GetEntity().Get<ecs::Transform>(lock);

                                    auto headPos = glm::vec3(xrObjectPos * glm::vec4(0, 0, 0, 1)) -
                                                   vrOriginTransform.GetPosition();
                                    auto newPos = origin - headPos;
                                    newPos += dir * float(std::max(0.0, hit.block.distance - 0.5));
                                    vrOriginTransform.SetPosition(
                                        glm::vec3(newPos.x, vrOriginTransform.GetPosition().y, newPos.z));
                                }
                            }
                        }

                        bool grab = false;
                        bool let_go = false;
                        grabAction->GetRisingEdgeActionValue(controllerAction.second, grab);
                        grabAction->GetFallingEdgeActionValue(controllerAction.second, let_go);

                        if (grab) {
                            auto lock = ecs::World.StartTransaction<
                                ecs::Read<ecs::HumanController>,
                                ecs::Write<ecs::PhysicsState, ecs::Transform, ecs::InteractController>>();

                            Logf("grab on subpath %s", controllerAction.second);
                            game->humanControlSystem.Interact(lock, xrObject.GetEntity());
                        } else if (let_go) {
                            Logf("Let go on subpath %s", controllerAction.second);
                            auto interact = xrObject.Get<ecs::InteractController>();
                            if (interact->target) {
                                interact->manager->RemoveConstraint(xrObject.GetEntity(), interact->target);
                                interact->target = nullptr;
                            }
                        }
    #endif
                    }
                }

                // Now move generic tracked objects around (HMD, Vive Pucks, etc..)
                for (xr::TrackedObjectHandle trackedObjectHandle : xrSystem->GetTracking()->GetTrackedObjectHandles()) {
                    ecs::Entity xrObject = ValidateAndLoadTrackedObject(trackedObjectHandle);

                    if (xrObject.Valid()) {
                        glm::mat4 xrObjectPos;

                        if (xrSystem->GetTracking()->GetPredictedObjectPose(trackedObjectHandle, xrObjectPos)) {
                            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::Transform>>();

                            auto &vrOriginTransformTecs = vrOrigin.GetEntity().Get<ecs::Transform>(lock);
                            xrObjectPos = glm::transpose(
                                xrObjectPos * glm::transpose(vrOriginTransformTecs.GetGlobalTransform(lock)));

                            auto &ctrl = xrObject.GetEntity().Get<ecs::Transform>(lock);
                            ctrl.SetPosition(xrObjectPos * glm::vec4(0, 0, 0, 1));
                            ctrl.SetRotate(glm::mat4(glm::mat3(xrObjectPos)));
                        }
                    }
                }

                if (CVarSkeletons.Get() != SkeletonMode::SkeletonDisabled) {
                    // Now load skeletons
                    for (xr::XrActionPtr action : {leftHandSkeletonAction, rightHandSkeletonAction}) {
                        // Get the action pose
                        glm::mat4 xrObjectPos;
                        bool activePose = action->GetPoseActionValueForNextFrame(xr::SubpathNone, xrObjectPos);

                        if (!activePose) {
                            // Can't do anything without a hand pose. Bail out early, destroying all skeleton
                            // entities on the way out
                            UpdateXrActionEntity(action, false);
                            continue;
                        }

                        {
                            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Transform>>();
                            auto &vrOriginTransformTecs = vrOrigin.GetEntity().Get<ecs::Transform>(lock);
                            xrObjectPos = glm::transpose(
                                xrObjectPos * glm::transpose(vrOriginTransformTecs.GetGlobalTransform(lock)));
                        }

                        // Get the bone data (optionally with or without an attached controller)
                        vector<xr::XrBoneData> boneData;
                        bool activeSkeleton = action->GetSkeletonActionValue(boneData, CVarController.Get());

                        if (!activeSkeleton) {
                            // Can't do anything without bone data. Bail out early, destroying all skeleton
                            // entities on the way out
                            UpdateXrActionEntity(action, false);
                            continue;
                        }

                        // Update the state of the "normal" skeleton entity (the RenderModel provided by the XR Runtime)
                        ecs::Entity handSkeleton = UpdateXrActionEntity(
                            action,
                            CVarSkeletons.Get() == SkeletonMode::SkeletonNormal);
                        if (handSkeleton.Valid()) {
                            auto hand = handSkeleton.Get<ecs::Renderable>();

                            ComputeBonePositions(boneData, hand->model->bones);

                            auto ctrl = handSkeleton.Get<ecs::Transform>();

                            // Extract just the position from the mat4
                            ctrl->SetPosition(xrObjectPos * glm::vec4(0, 0, 0, 1));
                            ctrl->SetRotate(glm::mat4(glm::mat3(xrObjectPos)));
                        }

                        // TODO: this checks the state of ~30 entities by name.
                        // Optimize this into separate functions for setup, teardown, and position updates. #39
                        UpdateSkeletonDebugHand(action,
                                                xrObjectPos,
                                                boneData,
                                                CVarSkeletons.Get() == SkeletonMode::SkeletonDebug);
                    }
                }
            }
        }

        return true;
    }

    void XrManager::InitXrActions() {
        gameActionSet = xrSystem->GetActionSet(xr::GameActionSet);

        // Create teleport action
        teleportAction = gameActionSet->CreateAction(xr::TeleportActionName, xr::XrActionType::Bool);

        // Suggested bindings for Oculus Touch
        teleportAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
                                            "/user/hand/right/input/a/click");

        // Suggested bindings for Valve Index
        teleportAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
                                            "/user/hand/right/input/trigger/click");

        // Suggested bindings for HTC Vive
        teleportAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
                                            "/user/hand/right/input/trackpad/click");

        // Create grab / interract action
        grabAction = gameActionSet->CreateAction(xr::GrabActionName, xr::XrActionType::Bool);

        // Suggested bindings for Oculus Touch
        grabAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
                                        "/user/hand/left/input/squeeze/value");
        grabAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
                                        "/user/hand/right/input/squeeze/value");

        // Suggested bindings for Valve Index
        grabAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
                                        "/user/hand/left/input/squeeze/click");
        grabAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
                                        "/user/hand/right/input/squeeze/click");

        // Suggested bindings for HTC Vive
        grabAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
                                        "/user/hand/left/input/squeeze/click");
        grabAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
                                        "/user/hand/right/input/squeeze/click");

        // Create LeftHand Pose action
        leftHandAction = gameActionSet->CreateAction(xr::LeftHandActionName, xr::XrActionType::Pose);
        leftHandAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
                                            "/user/hand/left/input/grip/pose");
        leftHandAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
                                            "/user/hand/left/input/grip/pose");
        leftHandAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
                                            "/user/hand/left/input/grip/pose");

        // Create RightHand Pose action
        rightHandAction = gameActionSet->CreateAction(xr::RightHandActionName, xr::XrActionType::Pose);
        rightHandAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
                                             "/user/hand/right/input/grip/pose");
        rightHandAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
                                             "/user/hand/right/input/grip/pose");
        rightHandAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
                                             "/user/hand/right/input/grip/pose");

        // Create LeftHand Skeleton action
        leftHandSkeletonAction = gameActionSet->CreateAction(xr::LeftHandSkeletonActionName,
                                                             xr::XrActionType::Skeleton);

        // TODO: add suggested bindings for real XR backends when OpenXR supports skeletons

        // Create RightHand Skeleton action
        rightHandSkeletonAction = gameActionSet->CreateAction(xr::RightHandSkeletonActionName,
                                                              xr::XrActionType::Skeleton);

        // TODO: add suggested bindings for real XR backends when OpenXR supports skeletons
    }

    void XrManager::LoadXrSystem() {
        InitXrActions();
    }

    // TODO: move this into XrSystem as part of #39
    void XrManager::ComputeBonePositions(vector<xr::XrBoneData> &boneData, vector<glm::mat4> &output) {
        // Resize the output vector to match the number of joints the GLTF will reference
        output.resize(boneData.size());

        for (size_t i = 0; i < boneData.size(); i++) {
            xr::XrBoneData &bone = boneData[i];

            glm::mat4 rot_mat = glm::mat4_cast(bone.rot);
            glm::mat4 trans_mat = glm::translate(bone.pos);
            glm::mat4 pose = trans_mat * rot_mat;
            output[i] = pose * bone.inverseBindPose;
        }
    }

    // TODO: move this into XrSystem as part of #39
    ecs::Entity XrManager::ValidateAndLoadTrackedObject(sp::xr::TrackedObjectHandle &trackedObjectHandle) {
        string entityName = trackedObjectHandle.name;
        ecs::Entity xrObject = game->entityManager.EntityWith<ecs::Name>(entityName);

        if (trackedObjectHandle.connected) {
            // Make sure object is valid
            if (!xrObject.Valid()) {
                xrObject = CreateXrEntity();
                xrObject.Assign<ecs::Name>(entityName);
            }

            if (!xrObject.Has<ecs::Transform>()) { xrObject.Assign<ecs::Transform>(); }

            if (!xrObject.Has<ecs::Renderable>()) {
                auto renderable = xrObject.Assign<ecs::Renderable>();
                renderable->model = xrSystem->GetTracking()->GetTrackedObjectModel(trackedObjectHandle);

                // TODO: better handling for failed model loads
                Assert((bool)(renderable->model), "Failed to load skeleton model");

                // Rendering an XR HMD model from the viewpoint of an XRView is a bad idea
                if (trackedObjectHandle.type == xr::HMD) {
                    renderable->visibility[ecs::Renderable::VISIBLE_DIRECT_EYE] = false;
                }
            }

            // Mark the XR HMD as being able to activate TriggerAreas
            if (trackedObjectHandle.type == xr::HMD && !xrObject.Has<ecs::Triggerable>()) {
                xrObject.Assign<ecs::Triggerable>();
            }
        } else {
            if (xrObject.Valid()) { xrObject.Destroy(); }
        }

        return xrObject;
    }

    // Validate and load the skeleton debug hand entities for a skeleton action. If the action is not "active", this
    // destroys the debug hand entities.
    void XrManager::UpdateSkeletonDebugHand(xr::XrActionPtr action,
                                            glm::mat4 xrObjectPos,
                                            vector<xr::XrBoneData> &boneData,
                                            bool active) {
        for (size_t i = 0; i < boneData.size(); i++) {
            xr::XrBoneData &bone = boneData[i];
            string entityName = string("xr-skeleton-debug-bone-") + action->GetName() + std::to_string(i);

            ecs::Entity boneEntity = game->entityManager.EntityWith<ecs::Name>(entityName);

            if (active) {
                if (!boneEntity.Valid()) {
                    boneEntity = CreateXrEntity();
                    boneEntity.Assign<ecs::Name>(entityName);
                }

                if (!boneEntity.Has<ecs::Transform>()) { boneEntity.Assign<ecs::Transform>(); }

    #ifdef SP_PHYSICS_SUPPORT_PHYSX
                if (!boneEntity.Has<ecs::InteractController>()) {
                    auto interact = boneEntity.Assign<ecs::InteractController>();
                    interact->manager = &game->physics;
                }
    #endif

                if (!boneEntity.Has<ecs::Renderable>()) {
                    auto model = GAssets.LoadModel("box");
                    auto renderable = boneEntity.Assign<ecs::Renderable>(model);

                    // TODO: better handling for failed model loads
                    Assert((bool)(renderable->model), "Failed to load skeleton model");
                }

                auto ctrl = boneEntity.Get<ecs::Transform>();
                ctrl->SetScale(glm::vec3(0.01f));
                ctrl->SetPosition(xrObjectPos * glm::vec4(bone.pos.x, bone.pos.y, bone.pos.z, 1));
                ctrl->SetRotate(glm::toMat4(bone.rot) * glm::mat4(glm::mat3(xrObjectPos)));
            } else {
                if (boneEntity.Valid()) { boneEntity.Destroy(); }
            }
        }
    }

    // Validate and load the entity and model associated with an action. If the action is not "active", this destroys
    // the entity and model.
    ecs::Entity XrManager::UpdateXrActionEntity(xr::XrActionPtr action, bool active) {
        string entityName = "xr-action-" + action->GetName();
        ecs::Entity xrObject = game->entityManager.EntityWith<ecs::Name>(entityName);

        // Test that the xrObject correctly reflects the "active" state
        if (active) {
            // Make sure object is valid
            if (!xrObject.Valid()) {
                xrObject = CreateXrEntity();
                xrObject.Assign<ecs::Name>(entityName);
            }

            if (!xrObject.Has<ecs::Transform>()) { xrObject.Assign<ecs::Transform>(); }

    #ifdef SP_PHYSICS_SUPPORT_PHYSX
            if (!xrObject.Has<ecs::InteractController>()) {
                auto interact = xrObject.Assign<ecs::InteractController>();
                interact->manager = &game->physics;
            }
    #endif

            // XrAction models might take many frames to load.
            // We constantly re-check the state of this entity while it's active
            // to continue trying to load the model from the underlying XR Runtime,
            // since it's likely being loaded from disk asyncrhonously and will eventually
            // become available.
            if (!xrObject.Has<ecs::Renderable>()) {
                shared_ptr<Model> inputSourceModel = action->GetInputSourceModel();

                if (inputSourceModel) {
                    auto renderable = xrObject.Assign<ecs::Renderable>();
                    renderable->model = inputSourceModel;
                }
            }
        }
        // Completely destroy inactive input sources so that resources are freed (if possible)
        else {
            if (xrObject.Valid()) { xrObject.Destroy(); }
        }

        return xrObject;
    }

    ecs::Entity XrManager::GetLaserPointer() {
        string entityName = "xr-laser-pointer";
        ecs::Entity xrObject = game->entityManager.EntityWith<ecs::Name>(entityName);

        // Make sure object is valid
        if (!xrObject.Valid()) {
            xrObject = CreateXrEntity();
            xrObject.Assign<ecs::Name>(entityName);
        }

        if (!xrObject.Has<ecs::Transform>()) { xrObject.Assign<ecs::Transform>(); }

        if (!xrObject.Has<ecs::Renderable>()) {
            auto renderable = xrObject.Assign<ecs::Renderable>();
            shared_ptr<BasicModel> model = make_shared<BasicModel>("laser-pointer-beam");
            renderable->model = model;

            // 10 unit long line
            glm::vec3 start = glm::vec3(0, 0, 0);
            glm::vec3 end = glm::vec3(0, 0, -10.0);
            float lineWidth = 0.001f;

            // Doesn't have to persist
            vector<SceneVertex> vertices;

            glm::vec3 lineDir = glm::normalize(end - start);
            glm::vec3 widthVec = lineWidth * glm::vec3(1.0, 0.0, 0.0);

            // move the positions back a bit to account for overlapping lines
            glm::vec3 pos0 = start; // start - lineWidth * lineDir;
            glm::vec3 pos1 = end + lineWidth * lineDir;

            auto addVertex = [&](const glm::vec3 &pos) {
                vertices.push_back({{pos.x, pos.y, pos.z}, glm::vec3(0.0, 1.0, 0.0), {0, 0}});
            };

            // 2 triangles that make up a "fat" line connecting pos0 and pos1
            // with the flat face pointing at the player
            addVertex(pos0 - widthVec);
            addVertex(pos1 + widthVec);
            addVertex(pos0 + widthVec);

            addVertex(pos1 - widthVec);
            addVertex(pos1 + widthVec);
            addVertex(pos0 - widthVec);

            // Create the data for the GPU
            // unsigned char baseColor[4] = {255, 0, 0, 255};

            // model->basicMaterials["red_laser"] = BasicMaterial(baseColor);
            // BasicMaterial &mat = model->basicMaterials["red_laser"];

            // GLModel::Primitive prim;

            // model->vbos.try_emplace("beam");
            // VertexBuffer &vbo = model->vbos["beam"];

            // model->ibos.try_emplace("beam");
            // GLBuffer &ibo = model->ibos["beam"];

            // const vector<uint16_t> indexData = {0, 1, 2, 3, 4, 5, 2, 1, 0, 5, 4, 3};

            // Model class will delete this on destruction
            // Model::Primitive *sourcePrim = new Model::Primitive;

            // prim.parent = sourcePrim;
            // prim.baseColorTex = &mat.baseColorTex;
            // prim.metallicRoughnessTex = &mat.metallicRoughnessTex;
            // prim.heightTex = &mat.heightTex;

            // vbo.SetElementsVAO(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
            // prim.vertexBufferHandle = vbo.VAO();

            // ibo.Create().Data(indexData.size() * sizeof(uint16_t), indexData.data());
            // prim.indexBufferHandle = ibo.handle;

            // sourcePrim->drawMode = Model::DrawMode::Triangles;
            // sourcePrim->indexBuffer.byteOffset = 0;
            // sourcePrim->indexBuffer.components = indexData.size();
            // sourcePrim->indexBuffer.componentType = GL_UNSIGNED_SHORT;
            // TODO(xthexder): Fix this
            // shared_ptr<GLModel> glModel = make_shared<GLModel>(renderable->model.get(),
            // game->graphics.GetRenderer()); glModel->AddPrimitive(prim); renderable->model->nativeModel = glModel;
        }

        return xrObject;
    }

    void XrManager::SetVrOrigin() {
        if (CVarConnectXR.Get()) {
            Logf("Resetting VR Origin");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Transform>>();
            auto vrOrigin = ecs::EntityWith<ecs::Name>(lock, "vr-origin");
            auto player = game->logic.GetPlayer();
            if (vrOrigin && vrOrigin.Has<ecs::Transform>(lock) && player && player.Has<ecs::Transform>(lock)) {
                auto &vrTransform = vrOrigin.Get<ecs::Transform>(lock);
                auto &playerTransform = player.Get<ecs::Transform>(lock);

                vrTransform.SetPosition(playerTransform.GetGlobalPosition(lock) -
                                        glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT, 0));
            }
        }
    }

    /**
     * @brief Helper function used when creating new entities that belong to
     * 		  GameLogic. Using this function ensures that the correct ecs::Creator
     * 		  attribute is added to each entity that is owned by GameLogic, and
     *		  therefore it gets destroyed on scene unload.
     *
     * @return ecs::Entity A new Entity with the ecs::Creator::GAME_LOGIC Key added.
     */
    inline ecs::Entity XrManager::CreateXrEntity() {
        auto newEntity = game->entityManager.NewEntity();
        newEntity.Assign<ecs::Owner>(ecs::Owner::SystemId::XR_MANAGER);
        return newEntity;
    }
}; // namespace sp::xr

#endif