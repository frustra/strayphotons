#pragma once

#include "ecs/Components.hh"

#include <string>
#include <vector>

namespace ecs {
    struct SceneConnection {
        std::vector<std::string> scenes;

        SceneConnection() {}
        SceneConnection(std::string scene) : scenes({scene}) {}

        bool HasScene(const std::string &sceneName) const;
    };

    static Component<SceneConnection> ComponentSceneConnection("scene_connection");

    template<>
    bool Component<SceneConnection>::Load(const EntityScope &scope, SceneConnection &dst, const picojson::value &src);
    template<>
    void Component<SceneConnection>::Apply(const SceneConnection &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
