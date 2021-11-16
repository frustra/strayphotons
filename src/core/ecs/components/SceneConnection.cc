#include "SceneConnection.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SceneConnection>::Load(sp::Scene *scene, SceneConnection &dst, const picojson::value &src) {
        if (scene != nullptr) dst.AddScene(scene->sceneName);
        if (src.is<std::string>()) {
            dst.AddScene(src.get<std::string>());
        } else if (src.is<picojson::array>()) {
            for (auto sceneName : src.get<picojson::array>()) {
                dst.AddScene(sceneName.get<std::string>());
            }
        }
        return true;
    }
} // namespace ecs
