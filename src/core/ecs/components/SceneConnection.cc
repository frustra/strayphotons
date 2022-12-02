#include "SceneConnection.hh"

#include "game/Scene.hh"

#include <algorithm>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    void Component<SceneConnection>::Apply(const SceneConnection &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstConnection = dst.Get<SceneConnection>(lock);
        for (auto &[scene, signals] : src.scenes) {
            auto &scenes = dstConnection.scenes[scene];
            scenes.insert(scenes.end(), signals.begin(), signals.end());
        }
    }
} // namespace ecs
