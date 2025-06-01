/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    struct GltfPrefab {
        std::string modelName;
        std::vector<std::string> skipNames;
        PhysicsGroup physicsGroup = PhysicsGroup::World;
        bool render = true;
        std::optional<ecs::PhysicsActorType> physicsType;
        bool interactive = false;

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            auto asyncGltf = sp::Assets().LoadGltf(modelName);
            auto model = asyncGltf->Get();
            if (!model) {
                Errorf("Gltf model not found: %s", modelName);
                return;
            }

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
                Entity newEntity = scene->NewPrefabEntity(lock,
                    ent,
                    state.GetInstanceId(),
                    getNodeName(nodeId),
                    prefixName);
                EntityRef newRef(newEntity);

                auto &transform = newEntity.Set<TransformTree>(lock, node.transform);
                if (parentEnt.Has<TransformTree>(lock) && parentEnt != newRef) {
                    transform.parent = parentEnt;
                }

                if (node.meshIndex) {
                    if (render) {
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

                    if (physicsType) {
                        PhysicsShape::ConvexMesh mesh(modelName, *node.meshIndex);
                        newEntity.Set<Physics>(lock, mesh, physicsGroup, *physicsType);

                        if (interactive) {
                            newEntity.Set<PhysicsJoints>(lock);
                            newEntity.Set<PhysicsQuery>(lock);
                            newEntity.Set<EventInput>(lock);
                            auto &scripts = newEntity.Set<Scripts>(lock);
                            scripts.AddScript(prefixName, "interactive_object");
                            GetScriptManager().RegisterEvents(lock, newEntity);
                        }
                    }
                }

                nodes.pop_front();
                for (auto &child : node.children) {
                    nodes.emplace_back(child, newEntity);
                }
            }
        }
    };
    StructMetadata MetadataGltfPrefab(typeid(GltfPrefab),
        sizeof(GltfPrefab),
        "GltfPrefab",
        "",
        StructField::New("model", &GltfPrefab::modelName),
        StructField::New("skip_nodes", &GltfPrefab::skipNames),
        StructField::New("physics_group", &GltfPrefab::physicsGroup),
        StructField::New("render", &GltfPrefab::render),
        StructField::New("physics", &GltfPrefab::physicsType),
        StructField::New("interactive", &GltfPrefab::interactive));
    PrefabScript<GltfPrefab> gltfPrefab("gltf", MetadataGltfPrefab);
} // namespace ecs
