#include "ecs/components/LightGun.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LightGun>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) {
        dst.Set<LightGun>(lock);
        return true;
    }

    LightGun::LightGun() {
        this->hasLight = false;
    }
} // namespace ecs
