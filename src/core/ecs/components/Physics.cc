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
                physics.model = sp::GAssets.LoadModel(param.second.get<string>());
            } else if (param.first == "dynamic") {
                physics.dynamic = param.second.get<bool>();
            } else if (param.first == "kinematic") {
                physics.kinematic = param.second.get<bool>();
            } else if (param.first == "decomposeHull") {
                physics.decomposeHull = param.second.get<bool>();
            } else if (param.first == "density") {
                physics.density = param.second.get<double>();
            } else if (param.first == "constraint") {
                Assert(scene, "Physics::Load must have valid scene to define constraint");
                std::string constraintTarget = "";
                ecs::PhysicsConstraintType constraintType = ecs::PhysicsConstraintType::Fixed;
                float constraintMaxDistance = 0.0f;
                Transform constraintTransform;
                if (param.second.is<string>()) {
                    constraintTarget = param.second.get<string>();
                } else if (param.second.is<picojson::object>()) {
                    for (auto constraintParam : param.second.get<picojson::object>()) {
                        if (constraintParam.first == "target") {
                            constraintTarget = constraintParam.second.get<string>();
                        } else if (constraintParam.first == "type") {
                            auto typeString = constraintParam.second.get<string>();
                            sp::to_upper(typeString);
                            if (typeString == "FIXED") {
                                constraintType = ecs::PhysicsConstraintType::Fixed;
                            } else if (typeString == "DISTANCE") {
                                constraintType = ecs::PhysicsConstraintType::Distance;
                            } else if (typeString == "SPHERICAL") {
                                constraintType = ecs::PhysicsConstraintType::Spherical;
                            } else if (typeString == "HINGE") {
                                constraintType = ecs::PhysicsConstraintType::Hinge;
                            } else if (typeString == "SLIDER") {
                                constraintType = ecs::PhysicsConstraintType::Slider;
                            } else if (typeString == "FORCELIMIT") {
                                constraintType = ecs::PhysicsConstraintType::ForceLimit;
                            } else {
                                Errorf("Unknown constraint type: %s", typeString);
                                return false;
                            }
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
                        constraintType,
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
