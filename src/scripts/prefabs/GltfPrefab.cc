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
        Assertf(ent.Has<Name>(lock), "Gltf prefab root has no name: %s", ToString(lock, ent));
        auto &prefixName = ent.Get<Name>(lock);

        auto getNodeName = [&](size_t nodeId) {
            auto &node = *model->nodes[nodeId];

            if (node.name.empty()) return "gltf" + std::to_string(nodeId);
            return node.name;
        };

        robin_hood::unordered_set<size_t> jointNodes;
        for (auto &skin : model->skins) {
            if (skin) {
                for (auto &joint : skin->joints) {
                    jointNodes.emplace(joint.jointNodeIndex);
                }
            }
        }

        static std::vector<std::string> emptyList;
        auto &skipNames = state.HasParam<decltype(emptyList)>("skip_nodes")
                              ? state.GetParamRef<decltype(emptyList)>("skip_nodes")
                              : emptyList;

        std::deque<std::pair<size_t, Entity>> nodes;
        for (auto &nodeId : model->rootNodes) {
            nodes.emplace_back(nodeId, ent);
        }
        while (!nodes.empty()) {
            auto &[nodeId, parentEnt] = nodes.front();
            Assertf(nodeId < model->nodes.size(), "Gltf root node id out of range: %u", nodeId);
            Assertf(model->nodes[nodeId], "Gltf node %u is not defined", nodeId);
            auto &node = *model->nodes[nodeId];

            auto nodeName = getNodeName(nodeId);
            if (sp::contains(skipNames, nodeName)) {
                nodes.pop_front();
                continue;
            }
            Entity newEntity = scene->NewPrefabEntity(lock, ent, getNodeName(nodeId), prefixName);

            TransformTree transform(node.transform);
            if (parentEnt.Has<TransformTree>(lock)) {
                transform.parent = parentEnt;
            } else {
                parentEnt = Entity();
            }
            LookupComponent<TransformTree>().ApplyComponent(transform, lock, newEntity);

            PhysicsGroup group = PhysicsGroup::World;
            auto physicsGroupParam = state.GetParam<std::string>("physics_group");
            if (!physicsGroupParam.empty()) {
                sp::to_upper(physicsGroupParam);
                if (physicsGroupParam == "NOCLIP") {
                    group = PhysicsGroup::NoClip;
                } else if (physicsGroupParam == "WORLD") {
                    group = PhysicsGroup::World;
                } else if (physicsGroupParam == "WORLD_OVERLAP") {
                    group = PhysicsGroup::WorldOverlap;
                } else if (physicsGroupParam == "INTERACTIVE") {
                    group = PhysicsGroup::Interactive;
                } else if (physicsGroupParam == "PLAYER") {
                    group = PhysicsGroup::Player;
                } else if (physicsGroupParam == "PLAYER_LEFT_HAND") {
                    group = PhysicsGroup::PlayerLeftHand;
                } else if (physicsGroupParam == "PLAYER_RIGHT_HAND") {
                    group = PhysicsGroup::PlayerRightHand;
                } else if (physicsGroupParam == "USER_INTERFACE") {
                    group = PhysicsGroup::UserInterface;
                } else {
                    Abortf("Unknown gltf physics group param: %s", physicsGroupParam);
                }
            }

            if (node.meshIndex) {
                if (state.GetParam<bool>("render")) {
                    Renderable renderable(modelName, asyncGltf, *node.meshIndex);

                    if (node.skinIndex) {
                        auto &skin = model->skins[*node.skinIndex];
                        if (!skin) {
                            Errorf("gltf %s missing skin %d", modelName, *node.skinIndex);
                        } else {
                            for (auto &j : skin->joints) {
                                renderable.joints.emplace_back(
                                    Renderable::Joint{ecs::Name(getNodeName(j.jointNodeIndex), prefixName),
                                        j.inverseBindPose});
                            }
                        }
                    }

                    LookupComponent<Renderable>().ApplyComponent(renderable, lock, newEntity);
                }

                auto physicsParam = state.GetParam<std::string>("physics");
                if (!physicsParam.empty()) {
                    sp::to_lower(physicsParam);
                    PhysicsShape::ConvexMesh mesh(modelName, *node.meshIndex);
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

                    LookupComponent<Physics>().ApplyComponent(physics, lock, newEntity);
                }
            }

            nodes.pop_front();
            for (auto &child : node.children) {
                nodes.emplace_back(child, newEntity);
            }
        }
    });
} // namespace ecs
