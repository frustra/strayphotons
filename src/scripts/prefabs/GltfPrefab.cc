#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    InternalPrefab gltfPrefab("gltf", [](ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        auto modelName = state.GetParam<std::string>("model");
        auto asyncGltf = sp::GAssets.LoadGltf(modelName);
        auto model = asyncGltf->Get();
        if (!model) {
            Errorf("Gltf model not found: %s", modelName);
            return;
        }

        auto scene = state.scene.lock();
        Assertf(scene, "Gltf prefab does not have a valid scene: %s", ToString(lock, ent));

        robin_hood::unordered_set<size_t> jointNodes;
        for (auto &skin : model->skins) {
            if (skin) {
                for (auto &joint : skin->joints) {
                    jointNodes.emplace(joint.jointNodeIndex);
                }
            }
        }

        std::deque<std::pair<size_t, Entity>> nodes;
        for (auto &nodeId : model->rootNodes) {
            nodes.emplace_back(nodeId, ent);
        }
        while (!nodes.empty()) {
            auto &[nodeId, parentEnt] = nodes.front();
            Assertf(nodeId < model->nodes.size(), "Gltf root node id out of range: %u", nodeId);
            Assertf(model->nodes[nodeId], "Gltf node %u is not defined", nodeId);
            auto &node = *model->nodes[nodeId];

            Entity newEntity;
            if (ent.Has<Name>(lock)) {
                auto name = ent.Get<Name>(lock);
                if (node.name.empty()) {
                    name.entity += ".gltf" + std::to_string(nodeId);
                } else {
                    name.entity += "." + node.name;
                }
                newEntity = scene->NewPrefabEntity(lock, ent, name);
            } else {
                newEntity = scene->NewPrefabEntity(lock, ent);
            }

            TransformTree transform(node.transform);
            if (parentEnt.Has<TransformTree>(lock)) transform.parentEntity = parentEnt;
            Component<TransformTree>::Apply(transform, lock, newEntity);

            PhysicsGroup group = PhysicsGroup::World;
            auto physicsGroupParam = state.GetParam<std::string>("physics_group");
            if (!physicsGroupParam.empty()) {
                sp::to_lower(physicsGroupParam);
                if (physicsGroupParam == "noclip") {
                    group = PhysicsGroup::NoClip;
                } else if (physicsGroupParam == "world") {
                    group = PhysicsGroup::World;
                } else if (physicsGroupParam == "interactive") {
                    group = PhysicsGroup::Interactive;
                } else if (physicsGroupParam == "player") {
                    group = PhysicsGroup::Player;
                } else if (physicsGroupParam == "player_hands") {
                    group = PhysicsGroup::PlayerHands;
                } else {
                    Abortf("Unknown gltf physics group param: %s", physicsGroupParam);
                }
            }

            if (node.meshIndex) {
                if (state.GetParam<bool>("render")) {
                    Renderable renderable(asyncGltf, *node.meshIndex);
                    Component<Renderable>::Apply(renderable, lock, newEntity);
                }

                auto physicsParam = state.GetParam<std::string>("physics");
                if (!physicsParam.empty()) {
                    sp::to_lower(physicsParam);
                    PhysicsShape::ConvexMesh mesh(asyncGltf, *node.meshIndex);
                    Physics physics(mesh);
                    if (physicsParam == "dynamic") {
                        physics.dynamic = true;
                    } else if (physicsParam == "kinematic") {
                        physics.dynamic = true;
                        physics.kinematic = true;
                    } else if (physicsParam == "static") {
                        physics.dynamic = false;
                    } else {
                        Abortf("Unknown gltf physics param: %s", physicsParam);
                    }
                    physics.group = group;
                    Component<Physics>::Apply(physics, lock, newEntity);
                }
            }

            auto jointsParam = state.GetParam<std::string>("physics_joints");
            if (!jointsParam.empty() && transform.parentEntity.Has<TransformTree>(lock)) {
                auto it = jointNodes.find(nodeId);
                if (it != jointNodes.end()) {
                    sp::to_lower(jointsParam);
                    auto parentTransform = transform.parentEntity.Get<TransformTree>(lock).GetGlobalTransform(lock);
                    auto globalTransform = transform.GetGlobalTransform(lock);
                    glm::vec3 boneVector = globalTransform.GetPosition() - parentTransform.GetPosition();
                    float boneLength = glm::length(boneVector);
                    Physics physics;
                    if (boneLength > 0.0f) {
                        physics.shape = PhysicsShape::Capsule(boneLength, 0.01f * globalTransform.GetScale().x);
                    } else {
                        physics.shape = PhysicsShape::Sphere(0.01f * globalTransform.GetScale().x);
                    }
                    if (jointsParam == "spherical") {
                        // physics.SetJoint(transform.parentEntity,
                        //     PhysicsJointType::Spherical,
                        //     glm::vec2(),
                        //     -boneVector);
                    } else if (jointsParam == "hinge") {
                        // physics.SetJoint(transform.parentEntity, PhysicsJointType::Hinge, glm::vec2(),
                        // -boneVector);
                    } else {
                        Abortf("Unknown physics_joints param: %s", jointsParam);
                    }
                    auto inversePose = glm::inverse(transform.pose.GetRotation());
                    physics.shapeTransform = Transform(inversePose * glm::vec3(boneLength * 0.5f, 0, 0), inversePose);
                    physics.group = group;
                    physics.kinematic = true;
                    Component<Physics>::Apply(physics, lock, newEntity);
                }
            }

            nodes.pop_front();
            for (auto &child : node.children) {
                nodes.emplace_back(child, newEntity);
            }
        }
    });
} // namespace ecs
