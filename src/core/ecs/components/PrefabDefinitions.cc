#include "Script.hh"
#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <robin_hood.h>

namespace ecs {
    robin_hood::unordered_node_map<std::string, PrefabFunc> PrefabDefinitions = {
        {"gltf",
            [](ScriptState &state, Lock<AddRemove> lock, Entity ent) {
                if (ent.Has<SceneInfo>(lock)) {
                    auto &sceneInfo = ent.Get<SceneInfo>(lock);

                    auto modelName = state.GetParam<std::string>("gltf_model");
                    auto asyncGltf = sp::GAssets.LoadGltf(modelName);
                    auto model = asyncGltf->Get();
                    if (!model) {
                        Errorf("Gltf model not found: %s", modelName);
                        return;
                    }

                    auto scene = sceneInfo.scene.lock();
                    Assertf(scene, "Gltf prefab does not have a valid scene: %s", ToString(lock, ent));

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
                            auto entityName = ent.Get<Name>(lock) + ".";
                            if (node.name.empty()) {
                                entityName += "gltf" + std::to_string(nodeId);
                            } else {
                                entityName += node.name;
                            }
                            newEntity = scene->NewPrefabEntity(lock, ent, entityName);
                        } else {
                            newEntity = scene->NewPrefabEntity(lock, ent);
                        }

                        TransformTree transform(node.transform);
                        if (parentEnt.Has<TransformTree>(lock)) transform.parent = parentEnt;
                        Component<TransformTree>::Apply(transform, lock, newEntity);

                        if (node.meshIndex) {
                            if (state.GetParam<bool>("gltf_render")) {
                                Renderable renderable(asyncGltf, *node.meshIndex);
                                ecs::Component<Renderable>::Apply(renderable, lock, newEntity);
                            }

                            auto physicsParam = state.GetParam<std::string>("gltf_physics");
                            if (!physicsParam.empty()) {
                                sp::to_lower(physicsParam);
                                Physics physics(asyncGltf, *node.meshIndex);
                                if (physicsParam == "dynamic") {
                                    physics.dynamic = true;
                                } else if (physicsParam == "kinematic") {
                                    physics.dynamic = true;
                                    physics.kinematic = true;
                                } else if (physicsParam == "static") {
                                    physics.dynamic = false;
                                } else {
                                    Abortf("Unknown gltf_physics param: %s", physicsParam);
                                }
                                ecs::Component<Physics>::Apply(physics, lock, newEntity);
                            }
                        }

                        nodes.pop_front();
                        for (auto &child : node.children) {
                            nodes.emplace_back(child, newEntity);
                        }
                    }
                }
            }},
    };
} // namespace ecs
