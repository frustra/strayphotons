#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    InternalPrefab gltfPrefab("gltf", [](const ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        auto modelName = state.GetParam<std::string>("model");
        auto asyncGltf = sp::Assets().LoadGltf(modelName);
        auto model = asyncGltf->Get();
        if (!model) {
            Errorf("Gltf model not found: %s", modelName);
            return;
        }

        auto scene = state.scope.scene.lock();
        Assertf(scene, "Gltf prefab does not have a valid scene: %s", ToString(lock, ent));
        Assertf(ent.Has<Name>(lock), "Gltf prefab root has no name: %s", ToString(lock, ent));
        auto prefixName = ent.Get<Name>(lock);

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
            Entity newEntity =
                scene->NewPrefabEntity(lock, ent, state.GetInstanceId(), getNodeName(nodeId), prefixName);

            auto &transform = newEntity.Set<TransformTree>(lock, node.transform);
            if (parentEnt.Has<TransformTree>(lock)) transform.parent = parentEnt;

            PhysicsGroup group = PhysicsGroup::World;
            auto physicsGroupParam = state.GetParam<std::string>("physics_group");
            if (!physicsGroupParam.empty()) {
                group = magic_enum::enum_cast<PhysicsGroup>(physicsGroupParam).value_or(group);
            }

            if (node.meshIndex) {
                if (state.GetParam<bool>("render")) {
                    auto &renderable = newEntity.Set<Renderable>(lock, modelName, asyncGltf, *node.meshIndex);

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
                }

                auto physicsParam = state.GetParam<std::string>("physics");
                if (!physicsParam.empty()) {
                    sp::to_lower(physicsParam);
                    PhysicsShape::ConvexMesh mesh(modelName, *node.meshIndex);
                    auto &physics = newEntity.Set<Physics>(lock, mesh, group);
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
                }
            }

            nodes.pop_front();
            for (auto &child : node.children) {
                nodes.emplace_back(child, newEntity);
            }
        }
    });
} // namespace ecs
