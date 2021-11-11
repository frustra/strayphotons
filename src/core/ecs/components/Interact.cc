#include "Interact.hh"

#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<InteractController>::Load(sp::Scene *scene, InteractController &dst, const picojson::value &src) {
        return true;
    }
} // namespace ecs
