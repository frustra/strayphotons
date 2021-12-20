#include "SceneConnection.hh"

#include "game/Scene.hh"

#include <algorithm>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SceneConnection>::Load(sp::Scene *scene, SceneConnection &dst, const picojson::value &src) {
        if (scene != nullptr) dst.scenes.emplace_back(scene->name);

        if (src.is<std::string>()) {
            dst.scenes.emplace_back(src.get<std::string>());
        } else if (src.is<picojson::array>()) {
            for (auto sceneParam : src.get<picojson::array>()) {
                dst.scenes.emplace_back(sceneParam.get<std::string>());
            }
        }
        return true;
    }

    bool SceneConnection::HasScene(const std::string &sceneName) const {
        return std::find(scenes.begin(), scenes.end(), sceneName) != scenes.end();
    }
} // namespace ecs
