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
            [](Lock<AddRemove> lock, Tecs::Entity ent) {
                if (ent.Has<Script, SceneInfo>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto &sceneInfo = ent.Get<SceneInfo>(lock);

                    auto modelName = scriptComp.GetParam<std::string>("gltf_model");
                    auto asyncGltf = sp::GAssets.LoadGltf(modelName);
                    auto model = asyncGltf->Get();
                    if (!model) {
                        Errorf("Gltf model not found: %s", modelName);
                        return;
                    }

                    auto scene = sceneInfo.scene.lock();
                    Assertf(scene, "Gltf prefab does not have a valid scene: %s", ToString(lock, ent));

                    std::deque<std::pair<size_t, Tecs::Entity>> nodes;
                    for (auto &nodeId : model->rootNodes) {
                        nodes.emplace_back(nodeId, ent);
                    }
                    while (!nodes.empty()) {
                        auto &[nodeId, parentEnt] = nodes.front();
                        Assertf(nodeId < model->nodes.size(), "Gltf root node id out of range: %u", nodeId);
                        Assertf(model->nodes[nodeId], "Gltf node %u is not defined", nodeId);
                        auto &node = *model->nodes[nodeId];

                        Tecs::Entity newEntity;
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

                        auto &transform = newEntity.Set<TransformTree>(lock, node.transform);
                        if (parentEnt.Has<TransformTree>(lock)) transform.parent = parentEnt;
                        newEntity.Set<TransformSnapshot>(lock);
                        if (node.meshIndex) {
                            if (scriptComp.GetParam<bool>("gltf_render")) {
                                newEntity.Set<Renderable>(lock, asyncGltf, *node.meshIndex);
                            }
                            auto physicsParam = scriptComp.GetParam<std::string>("gltf_physics");
                            sp::to_lower(physicsParam);
                            if (physicsParam == "dynamic") {
                                newEntity.Set<Physics>(lock, asyncGltf, *node.meshIndex);
                            } else if (physicsParam == "kinematic") {
                                auto &ph = newEntity.Set<Physics>(lock, asyncGltf, *node.meshIndex);
                                ph.kinematic = true;
                            } else if (physicsParam == "static") {
                                newEntity.Set<Physics>(lock, asyncGltf, *node.meshIndex, PhysicsGroup::World, false);
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
