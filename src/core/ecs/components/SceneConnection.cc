#include "SceneConnection.hh"

#include "game/Scene.hh"

#include <algorithm>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool StructMetadata::Load<SceneConnection>(const EntityScope &scope,
        SceneConnection &dst,
        const picojson::value &src) {
        if (src.is<std::string>()) {
            dst.scenes.emplace_back(src.get<std::string>());
        } else if (src.is<picojson::array>()) {
            for (auto sceneParam : src.get<picojson::array>()) {
                dst.scenes.emplace_back(sceneParam.get<std::string>());
            }
        }
        return true;
    }

    template<>
    void Component<SceneConnection>::Apply(const SceneConnection &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstConnection = dst.Get<SceneConnection>(lock);
        for (auto &scene : src.scenes) {
            if (!sp::contains(dstConnection.scenes, scene)) dstConnection.scenes.emplace_back(scene);
        }
    }
} // namespace ecs
