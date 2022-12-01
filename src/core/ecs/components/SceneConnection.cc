#include "SceneConnection.hh"

#include "game/Scene.hh"

#include <algorithm>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    void Component<SceneConnection>::Apply(const SceneConnection &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstConnection = dst.Get<SceneConnection>(lock);
        for (auto &scene : src.scenes) {
            if (!sp::contains(dstConnection.scenes, scene)) dstConnection.scenes.emplace_back(scene);
        }
    }
} // namespace ecs
