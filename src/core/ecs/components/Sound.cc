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
} // namespace ecs
