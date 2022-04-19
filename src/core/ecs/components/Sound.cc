#include "Sound.hh"

#include "assets/AssetHelpers.hh"
#include "assets/AssetManager.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Sound>::Load(const EntityScope &scope, Sound &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "file") {
                dst.file = sp::GAssets.Load("audio/" + param.second.get<string>());
            } else if (param.first == "type") {
                auto type = param.second.get<string>();
                sp::to_upper(type);
                if (type == "OBJECT") {
                    dst.type = Sound::Type::Object;
                } else if (type == "STEREO") {
                    dst.type = Sound::Type::Stereo;
                } else if (type == "AMBISONIC") {
                    dst.type = Sound::Type::Ambisonic;
                }
            } else if (param.first == "volume") {
                dst.volume = param.second.get<double>();
            }
        }
        return true;
    }
} // namespace ecs
