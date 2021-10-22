#include "Mirror.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Mirror>::Load(sp::Scene *scene, Mirror &mirror, const picojson::value &src) {
        mirror.size = sp::MakeVec2(src);
        return true;
    }
} // namespace ecs
