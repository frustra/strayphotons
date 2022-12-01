#pragma once

#include "ecs/Components.hh"

#include <string>
#include <vector>

namespace ecs {
    struct SceneConnection {
        std::vector<std::string> scenes;

        SceneConnection() {}
        SceneConnection(std::string scene) : scenes({scene}) {}
    };

    static StructMetadata MetadataSceneConnection(typeid(SceneConnection), StructField::New(&SceneConnection::scenes));
    static Component<SceneConnection> ComponentSceneConnection("scene_connection", MetadataSceneConnection);

    template<>
    void Component<SceneConnection>::Apply(const SceneConnection &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
