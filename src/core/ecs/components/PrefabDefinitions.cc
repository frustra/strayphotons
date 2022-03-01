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
                    if (parentEnt.Has<TransformTree>(lock)) transform.parent = parentEnt;
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
                            ecs::Component<Renderable>::Apply(renderable, lock, newEntity);
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
                            ecs::Component<Physics>::Apply(physics, lock, newEntity);
                        }
                    }

                    auto jointsParam = state.GetParam<std::string>("physics_joints");
                    if (!jointsParam.empty() && transform.parent) {
                        auto it = jointNodes.find(nodeId);
                        if (it != jointNodes.end()) {
                            sp::to_lower(jointsParam);
                            float boneLength = glm::length(transform.pose.GetPosition());
                            Physics physics;
                            if (boneLength > 0.0f) {
                                physics.shape = PhysicsShape::Capsule(boneLength, 0.01f);
                            } else {
                                physics.shape = PhysicsShape::Sphere(0.01f);
                            }
                            if (jointsParam == "spherical") {
                                physics.SetJoint(transform.parent,
                                    PhysicsJointType::Spherical,
                                    glm::vec2(),
                                    -transform.pose.GetPosition());
                            } else if (jointsParam == "hinge") {
                                physics.SetJoint(transform.parent,
                                    PhysicsJointType::Hinge,
                                    glm::vec2(),
                                    -transform.pose.GetPosition());
                            } else {
                                Abortf("Unknown physics_joints param: %s", jointsParam);
                            }
                            physics.shapeTransform = ecs::Transform(transform.pose.GetPosition() * -0.5f,
                                glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 0, 1)));
                            physics.group = group;
                            physics.kinematic = true;
                            ecs::Component<Physics>::Apply(physics, lock, newEntity);
                        }
                    }

                    nodes.pop_front();
                    for (auto &child : node.children) {
                        nodes.emplace_back(child, newEntity);
                    }
                }
            }},
    };
} // namespace ecs
