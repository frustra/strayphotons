#include "ecs/components/Mirror.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Mirror>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) {
        auto &mirror = dst.Set<Mirror>(lock);
        mirror.size = sp::MakeVec2(src);
        return true;
    }
} // namespace ecs
