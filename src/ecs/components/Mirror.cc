#include "ecs/components/Mirror.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Mirror>::Load(Lock<Read<ecs::Name>> lock, Mirror &mirror, const picojson::value &src) {
        mirror.size = sp::MakeVec2(src);
        return true;
    }
} // namespace ecs
