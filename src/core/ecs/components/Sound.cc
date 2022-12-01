#include "Sound.hh"

#include "assets/AssetManager.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool StructMetadata::Load<Sound>(const EntityScope &scope, Sound &sound, const picojson::value &src) {
        if (!sound.filePath.empty()) {
            sound.file = sp::Assets().Load("audio/" + sound.filePath);
        }
        return true;
    }

    template<>
    void Component<Sounds>::Apply(const Sounds &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstSounds = dst.Get<Sounds>(lock);
        for (auto &sound : src.sounds) {
            if (!sp::contains(dstSounds.sounds, sound)) dstSounds.sounds.emplace_back(sound);
        }
    }
} // namespace ecs
