#pragma once

#include "ecs/Components.hh"

#include <functional>
#include <string>
#include <vector>

namespace ecs {
    struct SceneConnection {
        std::vector<std::string> sceneNames;

        bool ContainsScene(std::string name) const {
            return std::find(sceneNames.begin(), sceneNames.end(), name) != sceneNames.end();
        }

        void AddScene(std::string name) {
            if (!ContainsScene(name)) sceneNames.emplace_back(name);
        }
    };

    static Component<SceneConnection> ComponentSceneConnection("scene_connection");

    template<>
    bool Component<SceneConnection>::Load(sp::Scene *scene, SceneConnection &dst, const picojson::value &src);
} // namespace ecs
