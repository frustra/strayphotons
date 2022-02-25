#include "Physics.hh"

#include "game/Scene.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Physics>::Load(sp::Scene *scene, Physics &physics, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "model") {
                physics.model = sp::GAssets.LoadGltf(param.second.get<string>());
            } else if (param.first == "mesh") {
                physics.meshIndex = (size_t)param.second.get<double>();
            } else if (param.first == "dynamic") {
                physics.dynamic = param.second.get<bool>();
            } else if (param.first == "kinematic") {
                physics.kinematic = param.second.get<bool>();
            } else if (param.first == "decomposeHull") {
                physics.decomposeHull = param.second.get<bool>();
            } else if (param.first == "density") {
                physics.density = param.second.get<double>();
            } else if (param.first == "group") {
                auto groupString = param.second.get<string>();
                sp::to_upper(groupString);
                if (groupString == "NOCLIP") {
                    physics.group = ecs::PhysicsGroup::NoClip;
                } else if (groupString == "WORLD") {
                    physics.group = ecs::PhysicsGroup::World;
                } else if (groupString == "INTERACTIVE") {
                    physics.group = ecs::PhysicsGroup::Interactive;
                } else if (groupString == "PLAYER") {
                    physics.group = ecs::PhysicsGroup::Player;
                } else if (groupString == "PLAYER_HANDS") {
                    physics.group = ecs::PhysicsGroup::PlayerHands;
                } else {
                    Errorf("Unknown physics group: %s", groupString);
                    return false;
                }
            } else if (param.first == "joint") {
                Assert(scene, "Physics::Load must have valid scene to define joint");
                std::string jointTarget = "";
                ecs::PhysicsJointType jointType = ecs::PhysicsJointType::Fixed;
                glm::vec2 jointRange;
                ecs::Transform localTransform, remoteTransform;
                for (auto jointParam : param.second.get<picojson::object>()) {
                    if (jointParam.first == "target") {
                        jointTarget = jointParam.second.get<string>();
                    } else if (jointParam.first == "type") {
                        auto typeString = jointParam.second.get<string>();
                        sp::to_upper(typeString);
                        if (typeString == "FIXED") {
                            jointType = ecs::PhysicsJointType::Fixed;
                        } else if (typeString == "DISTANCE") {
                            jointType = ecs::PhysicsJointType::Distance;
                        } else if (typeString == "SPHERICAL") {
                            jointType = ecs::PhysicsJointType::Spherical;
                        } else if (typeString == "HINGE") {
                            jointType = ecs::PhysicsJointType::Hinge;
                        } else if (typeString == "SLIDER") {
                            jointType = ecs::PhysicsJointType::Slider;
                        } else {
                            Errorf("Unknown joint type: %s", typeString);
                            return false;
                        }
                    } else if (jointParam.first == "range") {
                        jointRange = sp::MakeVec2(jointParam.second);
                    } else if (jointParam.first == "local_offset") {
                        if (!Component<Transform>::Load(scene, localTransform, jointParam.second)) {
                            Errorf("Couldn't parse physics joint local_offset as Transform");
                            return false;
                        }
                    } else if (jointParam.first == "remote_offset") {
                        if (!Component<Transform>::Load(scene, remoteTransform, jointParam.second)) {
                            Errorf("Couldn't parse physics joint remote_offset as Transform");
                            return false;
                        }
                    }
                }
                auto it = scene->namedEntities.find(jointTarget);
                if (it != scene->namedEntities.end()) {
                    physics.SetJoint(it->second,
                        jointType,
                        jointRange,
                        localTransform.GetPosition(),
                        localTransform.GetRotation(),
                        remoteTransform.GetPosition(),
                        remoteTransform.GetRotation());
                } else {
                    Errorf("Component<Physics>::Load joint name does not exist: %s", jointTarget);
                    return false;
                }
            } else if (param.first == "force") {
                physics.constantForce = sp::MakeVec3(param.second);
            } else if (param.first == "constraint") {
                Assert(scene, "Physics::Load must have valid scene to define constraint");
                std::string constraintTarget = "";
                float constraintMaxDistance = 0.0f;
                Transform constraintTransform;
                if (param.second.is<string>()) {
                    constraintTarget = param.second.get<string>();
                } else if (param.second.is<picojson::object>()) {
                    for (auto constraintParam : param.second.get<picojson::object>()) {
                        if (constraintParam.first == "target") {
                            constraintTarget = constraintParam.second.get<string>();
                        } else if (constraintParam.first == "break_distance") {
                            constraintMaxDistance = constraintParam.second.get<double>();
                        } else if (constraintParam.first == "offset") {
                            if (!Component<Transform>::Load(scene, constraintTransform, constraintParam.second)) {
                                Errorf("Couldn't parse physics constraint offset as Transform");
                                return false;
                            }
                        }
                    }
                }
                auto it = scene->namedEntities.find(constraintTarget);
                if (it != scene->namedEntities.end()) {
                    physics.SetConstraint(it->second,
                        constraintMaxDistance,
                        constraintTransform.GetPosition(),
                        constraintTransform.GetRotation());
                } else {
                    Errorf("Component<Physics>::Load constraint name does not exist: %s", constraintTarget);
                    return false;
                }
            }
        }
        return true;
    }

    template<>
    bool Component<PhysicsQuery>::Load(sp::Scene *scene, PhysicsQuery &query, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "raycast") query.raycastQueryDistance = param.second.get<double>();
        }
        return true;
    }
} // namespace ecs
