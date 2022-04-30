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

        auto scene = state.scope.scene.lock();
        Assertf(scene, "Gltf prefab does not have a valid scene: %s", ToString(lock, ent));

        auto getNodeName = [&](size_t nodeId) {
            auto &node = *model->nodes[nodeId];
            if (!ent.Has<Name>(lock)) return ecs::Name();

            auto name = ent.Get<Name>(lock);
            if (node.name.empty()) {
                name.entity += ".gltf" + std::to_string(nodeId);
            } else {
                name.entity += "." + node.name;
            }
            return name;
        };

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

            Entity newEntity = scene->NewPrefabEntity(lock, ent, getNodeName(nodeId));

            TransformTree transform(node.transform);
            if (parentEnt.Has<TransformTree>(lock)) {
                transform.parent = parentEnt;
            } else {
                parentEnt = Entity();
            }
            Component<TransformTree>::Apply(transform, lock, newEntity);

            PhysicsGroup group = PhysicsGroup::World;
            auto physicsGroupParam = state.GetParam<std::string>("physics_group");
            if (!physicsGroupParam.empty()) {
                sp::to_upper(physicsGroupParam);
                if (physicsGroupParam == "NOCLIP") {
                    group = PhysicsGroup::NoClip;
                } else if (physicsGroupParam == "WORLD") {
                    group = PhysicsGroup::World;
                } else if (physicsGroupParam == "INTERACTIVE") {
                    group = PhysicsGroup::Interactive;
                } else if (physicsGroupParam == "HELD_OBJECT") {
                    group = PhysicsGroup::HeldObject;
                } else if (physicsGroupParam == "PLAYER") {
                    group = PhysicsGroup::Player;
                } else if (physicsGroupParam == "PLAYER_LEFT_HAND") {
                    group = PhysicsGroup::PlayerLeftHand;
                } else if (physicsGroupParam == "PLAYER_RIGHT_HAND") {
                    group = PhysicsGroup::PlayerRightHand;
                } else {
                    Abortf("Unknown gltf physics group param: %s", physicsGroupParam);
                }
            }

            if (node.meshIndex) {
                if (state.GetParam<bool>("render")) {
                    Renderable renderable(asyncGltf, *node.meshIndex);

                    if (node.skinIndex) {
                        auto &skin = model->skins[*node.skinIndex];
                        if (!skin) {
                            Errorf("gltf %s missing skin %d", modelName, *node.skinIndex);
                        } else {
                            for (auto &j : skin->joints) {
                                renderable.joints.emplace_back(
                                    Renderable::Joint{getNodeName(j.jointNodeIndex), j.inverseBindPose});
                            }
                        }
                    }
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

            nodes.pop_front();
            for (auto &child : node.children) {
                nodes.emplace_back(child, newEntity);
            }
        }
    });
} // namespace ecs
