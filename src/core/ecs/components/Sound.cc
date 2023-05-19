#include "Sound.hh"

#include "assets/AssetManager.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool StructMetadata::Load<Sound>(Sound &sound, const picojson::value &src) {
        if (!sound.filePath.empty()) {
            sound.file = sp::Assets().Load("audio/" + sound.filePath);
        }
        return true;
    }

    template<>
    void Component<Sounds>::Apply(Sounds &dst, const Sounds &src, bool liveTarget) {
        for (auto &sound : src.sounds) {
            if (!sp::contains(dst.sounds, sound)) dst.sounds.emplace_back(sound);
        }
    }
} // namespace ecs
