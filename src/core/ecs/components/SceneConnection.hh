#pragma once

#include "ecs/Components.hh"

namespace ecs {
    struct SceneConnection {};

    static Component<SceneConnection> ComponentSceneConnection("scene_connection");

    template<>
    bool Component<SceneConnection>::Load(sp::Scene *scene, SceneConnection &dst, const picojson::value &src);
} // namespace ecs
