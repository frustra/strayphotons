#include "Sound.hh"

#include "assets/AssetManager.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool StructMetadata::Load<Sounds>(const EntityScope &scope, Sounds &dst, const picojson::value &src) {
        auto parseObject = [&](const picojson::value &src, Sound &dst) {
            for (auto &param : src.get<picojson::object>()) {
                if (param.first == "file") {
                    dst.file = sp::Assets().Load("audio/" + param.second.get<string>());
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
                } else if (param.first == "loop") {
                    dst.loop = param.second.get<bool>();
                } else if (param.first == "play_on_load") {
                    dst.playOnLoad = param.second.get<bool>();
                }
            }
        };

        if (src.is<picojson::array>()) {
            for (auto &value : src.get<picojson::array>()) {
                dst.sounds.emplace_back();
                parseObject(value, dst.sounds.back());
            }
        } else {
            dst.sounds.emplace_back();
            parseObject(src, dst.sounds.back());
        }
        return true;
    }

    template<>
    void Component<Sounds>::Apply(const Sounds &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstSounds = dst.Get<Sounds>(lock);
        if (dstSounds.sounds.empty()) dstSounds.sounds = src.sounds;
    }
} // namespace ecs
