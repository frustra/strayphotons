#include "Physics.hh"

#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "assets/PhysicsInfo.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    template<>
    bool StructMetadata::Load<PhysicsShape>(const EntityScope &scope, PhysicsShape &shape, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "model") {
                if (shape) {
                    Errorf("PhysicShape defines multiple shapes: model");
                    return false;
                }
                if (param.second.is<std::string>()) {
                    shape.shape = PhysicsShape::ConvexMesh(param.second.get<std::string>());
                } else {
                    Errorf("Unknown physics model value: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "plane") {
                if (shape) {
                    Errorf("PhysicShape defines multiple shapes: plane");
                    return false;
                }
                shape.shape = PhysicsShape::Plane();
            } else if (param.first == "capsule") {
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
    void StructMetadata::Save<PhysicsShape>(const EntityScope &scope,
        picojson::value &dst,
        const PhysicsShape &src,
        const PhysicsShape &def) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        if (std::holds_alternative<PhysicsShape::Sphere>(src.shape)) {
            auto &sphere = std::get<PhysicsShape::Sphere>(src.shape);
            sp::json::Save(scope, obj["sphere"], sphere.radius);
        } else if (std::holds_alternative<PhysicsShape::Capsule>(src.shape)) {
            static const PhysicsShape::Capsule defaultCapsule = {};

            auto &capsule = std::get<PhysicsShape::Capsule>(src.shape);
            picojson::object capsuleObj = {};
            sp::json::SaveIfChanged(scope, capsuleObj, "radius", capsule.radius, defaultCapsule.radius);
            sp::json::SaveIfChanged(scope, capsuleObj, "height", capsule.height, defaultCapsule.height);
            obj["capsule"] = picojson::value(capsuleObj);
        } else if (std::holds_alternative<PhysicsShape::Box>(src.shape)) {
            auto &box = std::get<PhysicsShape::Box>(src.shape);
            sp::json::Save(scope, obj["box"], box.extents);
        } else if (std::holds_alternative<PhysicsShape::Plane>(src.shape)) {
            obj["plane"].set<picojson::object>({});
        } else if (std::holds_alternative<PhysicsShape::ConvexMesh>(src.shape)) {
            auto &model = std::get<PhysicsShape::ConvexMesh>(src.shape);
            auto fullModelName = model.modelName;
            if (model.meshName != "convex0") fullModelName += "." + model.meshName;
            sp::json::Save(scope, obj["model"], fullModelName);
        } else {
            Abortf("Unknown PhysicsShape type: %u", src.shape.index());
        }
        sp::json::SaveIfChanged(scope, obj, "transform", src.transform, {});
    }

    PhysicsShape::ConvexMesh::ConvexMesh(const std::string &fullMeshName) {
        auto sep = fullMeshName.find('.');
        if (sep != std::string::npos) {
            modelName = fullMeshName.substr(0, sep);
            meshName = fullMeshName.substr(sep + 1);
        } else {
            modelName = fullMeshName;
            meshName = "convex0";
        }

        Assertf(!modelName.empty(), "ConvexMesh created with empty model name");
        Assertf(!meshName.empty(), "ConvexMesh created with empty mesh name");
        model = sp::Assets().LoadGltf(modelName);
        hullSettings = sp::Assets().LoadHullSettings(modelName, meshName);
    }

    PhysicsShape::ConvexMesh::ConvexMesh(const std::string &modelName, const std::string &meshName)
        : modelName(modelName), meshName(meshName) {
        Assertf(!modelName.empty(), "ConvexMesh created with empty model name");
        Assertf(!meshName.empty(), "ConvexMesh created with empty mesh name");
        model = sp::Assets().LoadGltf(modelName);
        hullSettings = sp::Assets().LoadHullSettings(modelName, meshName);
    }
} // namespace ecs
