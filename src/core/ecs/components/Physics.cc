#include "Physics.hh"

#include "game/Scene.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

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
            } else if (param.first == "transform") {
                Transform shapeTransform;
                if (Component<Transform>::Load(scope, shapeTransform, param.second)) {
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
        for (auto param : src.get<picojson::object>()) {
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
                PhysicsShape::ConvexMesh mesh;
                if (param.second.is<std::string>()) {
                    mesh = PhysicsShape::ConvexMesh(sp::GAssets.LoadGltf(param.second.get<std::string>()));
                } else if (param.second.is<picojson::object>()) {
                    for (auto meshParam : param.second.get<picojson::object>()) {
                        if (meshParam.first == "model") {
                            mesh.model = sp::GAssets.LoadGltf(meshParam.second.get<std::string>());
                        } else if (meshParam.first == "mesh_index") {
                            mesh.meshIndex = (size_t)meshParam.second.get<double>();
                        } else {
                            Errorf("Unknown physics model field: %s", meshParam.first);
                            return false;
                        }
                    }
                } else {
                    Errorf("Unknown physics model value: %s", param.second.to_str());
                    return false;
                }
                physics.shapes.emplace_back(mesh);
            } else if (param.first == "dynamic") {
                physics.dynamic = param.second.get<bool>();
            } else if (param.first == "kinematic") {
                physics.kinematic = param.second.get<bool>();
            } else if (param.first == "decomposeHull") {
                physics.decomposeHull = param.second.get<bool>();
            } else if (param.first == "density") {
                physics.density = param.second.get<double>();
            } else if (param.first == "angular_damping") {
                physics.angularDamping = param.second.get<double>();
            } else if (param.first == "linear_damping") {
                physics.linearDamping = param.second.get<double>();
            } else if (param.first == "group") {
                auto groupString = param.second.get<string>();
                sp::to_upper(groupString);
                if (groupString == "NOCLIP") {
                    physics.group = PhysicsGroup::NoClip;
                } else if (groupString == "WORLD") {
                    physics.group = PhysicsGroup::World;
                } else if (groupString == "INTERACTIVE") {
                    physics.group = PhysicsGroup::Interactive;
                } else if (groupString == "PLAYER") {
                    physics.group = PhysicsGroup::Player;
                } else if (groupString == "PLAYER_HANDS") {
                    physics.group = PhysicsGroup::PlayerHands;
                } else {
                    Errorf("Unknown physics group: %s", groupString);
                    return false;
                }
            } else if (param.first == "force") {
                physics.constantForce = sp::MakeVec3(param.second);
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
                            if (!Component<Transform>::Load(scope, constraintTransform, constraintParam.second)) {
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
            } else {
                Errorf("Unknown physics param: %s", param.first);
                return false;
            }
        }
        return true;
    }

    template<>
    bool Component<PhysicsQuery>::Load(const EntityScope &scope, PhysicsQuery &query, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "raycast") { query.raycastQueryDistance = param.second.get<double>(); }
        }
        return true;
    }
} // namespace ecs
