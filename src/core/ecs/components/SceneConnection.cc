#include "SceneConnection.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SceneConnection>::Load(sp::Scene *scene, SceneConnection &dst, const picojson::value &src) {
        return true;
    }
} // namespace ecs
