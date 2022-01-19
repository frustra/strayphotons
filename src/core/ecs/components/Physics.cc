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
                auto constraintName = param.second.get<string>();
                auto it = scene->namedEntities.find(constraintName);
                if (it != scene->namedEntities.end()) {
                    physics.constraint = it->second;
                } else {
                    Errorf("Component<Physics>::Load constraint name does not exist: %s", constraintName);
                }
            }
        }
        return true;
    }

    template<>
    bool Component<PhysicsQuery>::Load(sp::Scene *scene, PhysicsQuery &query, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "raycast") query.maxRaycastDistance = param.second.get<double>();
        }
        return true;
    }
} // namespace ecs
