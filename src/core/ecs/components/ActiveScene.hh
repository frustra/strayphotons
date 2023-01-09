#pragma once

#include "game/SceneRef.hh"

namespace ecs {
    struct ActiveScene {
        sp::SceneRef scene;

        ActiveScene() {}
        ActiveScene(const sp::SceneRef &scene) : scene(scene) {}
    };
} // namespace ecs

TECS_GLOBAL_COMPONENT(ecs::ActiveScene);
