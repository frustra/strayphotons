#include "SceneConnection.hh"

#include "game/Scene.hh"

#include <algorithm>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    void Component<SceneConnection>::Apply(SceneConnection &dst, const SceneConnection &src, bool liveTarget) {
        if (liveTarget) {
            dst = src;
        } else {
            for (auto &[scene, signals] : src.scenes) {
                auto &scenes = dst.scenes[scene];
                scenes.insert(scenes.end(), signals.begin(), signals.end());
            }
        }
    }
} // namespace ecs
