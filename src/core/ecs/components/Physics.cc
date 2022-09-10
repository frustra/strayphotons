#include "Physics.hh"

#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "assets/PhysicsInfo.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    bool parsePhysicsShape(const EntityScope &scope, PhysicsShape &shape, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "capsule") {
                if (shape) {
                    Errorf("PhysicShape defines multiple shapes: capsule");
                    return false;
                }
                if (param.second.is<picojson::object>()) {
                    PhysicsShape::Capsule capsule;
                    for (auto capsuleParam : param.second.get<picojson::object>()) {
                        if (capsuleParam.first == "radius") {
                            capsule.radius = capsuleParam.second.get<double>();
                        } else if (capsuleParam.first == "height") {
                            capsule.height = capsuleParam.second.get<double>();
                        } else {
                            Errorf("Unknown physics capsule field: %s", capsuleParam.first);
                            return false;
                        }
                    }
                    shape.shape = capsule;
                } else {
                    Errorf("Unknown physics capsule value: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "sphere") {
                if (shape) {
                    Errorf("PhysicShape defines multiple shapes: sphere");
                    return false;
                }
                PhysicsShape::Sphere sphere;
                if (param.second.is<double>()) {
                    sphere.radius = param.second.get<double>();
                } else if (param.second.is<picojson::object>()) {
                    for (auto sphereParam : param.second.get<picojson::object>()) {
                        if (sphereParam.first == "radius") {
                            sphere.radius = sphereParam.second.get<double>();
                        } else {
                            Errorf("Unknown physics sphere field: %s", sphereParam.first);
                            return false;
                        }
                    }
                } else {
                    Errorf("Unknown physics sphere value: %s", param.second.to_str());
                    return false;
                }
                shape.shape = sphere;
            } else if (param.first == "box") {
                if (shape) {
                    Errorf("PhysicShape defines multiple shapes: box");
                    return false;
                }
                PhysicsShape::Box box;
                if (param.second.is<picojson::array>()) {
                    if (!sp::json::Load(scope, box.extents, param.second)) {
                        Errorf("Invalid physics box extents: %s", param.second.to_str());
                        return false;
                    }
                    shape.shape = box;
                } else {
                    Errorf("Unknown physics box value: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "transform") {
                Transform shapeTransform;
                if (sp::json::Load(scope, shapeTransform, param.second)) {
                    shape.transform = shapeTransform;
                } else {
                    Errorf("Couldn't parse PhysicsShape transform");
                    return false;
                }
            } else {
                Errorf("Unknown PhysicsShape field: %s", param.first);
                return false;
            }
        }
        if (!shape) {
            Errorf("PhysicShape doesn't define a shape type");
            return false;
        }
        return true;
    }

    template<>
    bool Component<Physics>::Load(const EntityScope &scope, Physics &physics, const picojson::value &src) {
        auto scene = scope.scene.lock();
        Assert(scene, "Physics::Load must have valid scene");
        for (auto param : src.get<picojson::object>()) {
            if (sp::starts_with(param.first, "_")) continue;
            if (param.first == "shapes") {
                if (param.second.is<picojson::object>()) {
                    PhysicsShape shape;
                    if (!parsePhysicsShape(scope, shape, param.second)) return false;
                    physics.shapes.emplace_back(shape);
                } else if (param.second.is<picojson::array>()) {
                    for (auto shapeParam : param.second.get<picojson::array>()) {
                        PhysicsShape shape;
                        if (!parsePhysicsShape(scope, shape, shapeParam)) return false;
                        physics.shapes.emplace_back(shape);
                    }
                }
            } else if (param.first == "model") {
                if (std::find_if(physics.shapes.begin(), physics.shapes.end(), [](auto &&shape) {
                        return std::holds_alternative<PhysicsShape::ConvexMesh>(shape.shape);
                    }) != physics.shapes.end()) {
                    Errorf("PhysicShape defines multiple shapes: model");
                    return false;
                }
                if (param.second.is<std::string>()) {
                    physics.shapes.emplace_back(param.second.get<std::string>());
                } else {
                    Errorf("Unknown physics model value: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "group") {
                auto groupString = param.second.get<string>();
                sp::to_upper(groupString);
                if (groupString == "NOCLIP") {
                    physics.group = PhysicsGroup::NoClip;
                } else if (groupString == "WORLD") {
                    physics.group = PhysicsGroup::World;
                } else if (groupString == "WORLD_OVERLAP") {
                    physics.group = PhysicsGroup::WorldOverlap;
                } else if (groupString == "INTERACTIVE") {
                    physics.group = PhysicsGroup::Interactive;
                } else if (groupString == "PLAYER") {
                    physics.group = PhysicsGroup::Player;
                } else if (groupString == "PLAYER_LEFT_HAND") {
                    physics.group = PhysicsGroup::PlayerLeftHand;
                } else if (groupString == "PLAYER_RIGHT_HAND") {
                    physics.group = PhysicsGroup::PlayerRightHand;
                } else if (groupString == "USER_INTERFACE") {
                    physics.group = PhysicsGroup::UserInterface;
                } else {
                    Errorf("Unknown physics group: %s", groupString);
                    return false;
                }
            } else if (param.first == "constraint") {
                Assert(scene, "Physics::Load must have valid scene to define constraint");
                Name constraintTargetName;
                float constraintMaxDistance = 0.0f;
                Transform constraintTransform;
                if (param.second.is<string>()) {
                    constraintTargetName = Name(param.second.get<string>(), scope.prefix);
                } else if (param.second.is<picojson::object>()) {
                    for (auto constraintParam : param.second.get<picojson::object>()) {
                        if (constraintParam.first == "target") {
                            constraintTargetName = Name(constraintParam.second.get<string>(), scope.prefix);
                        } else if (constraintParam.first == "break_distance") {
                            constraintMaxDistance = constraintParam.second.get<double>();
                        } else if (constraintParam.first == "offset") {
                            if (!sp::json::Load(scope, constraintTransform, constraintParam.second)) {
                                Errorf("Couldn't parse physics constraint offset as Transform");
                                return false;
                            }
                        }
                    }
                }
                if (constraintTargetName) {
                    physics.SetConstraint(constraintTargetName,
                        constraintMaxDistance,
                        constraintTransform.GetPosition(),
                        constraintTransform.GetRotation());
                } else {
                    Errorf("Component<Physics>::Load constraint name does not exist: %s",
                        constraintTargetName.String());
                    return false;
                }
            }
        }
        return true;
    }

    PhysicsShape::ConvexMesh::ConvexMesh(const std::string &modelName, const std::string &meshName)
        : modelName(modelName), meshName(meshName) {
        Assertf(!modelName.empty(), "ConvexMesh created with empty model name");
        Assertf(!meshName.empty(), "ConvexMesh created with empty mesh name");
        model = sp::Assets().LoadGltf(modelName);
        hullSettings = sp::Assets().LoadHullSettings(modelName, meshName);
    }

    PhysicsShape::PhysicsShape(const std::string &fullMeshName) {
        auto sep = fullMeshName.find('.');
        std::string modelName, meshName;
        if (sep != std::string::npos) {
            modelName = fullMeshName.substr(0, sep);
            meshName = fullMeshName.substr(sep + 1);
        } else {
            modelName = fullMeshName;
            meshName = "convex0";
        }
        shape = ConvexMesh(modelName, meshName);
    }
} // namespace ecs
