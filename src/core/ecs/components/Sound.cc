#include "Sound.hh"

#include "assets/AssetHelpers.hh"
#include "assets/AssetManager.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Sound>::Load(ScenePtr scenePtr, Sound &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "file") dst.file = sp::GAssets.Load("audio/" + param.second.get<string>() + ".ogg");
        }
        return true;
    }
} // namespace ecs
