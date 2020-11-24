#include "ecs/components/LightGun.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LightGun>::Load(Lock<Read<ecs::Name>> lock, LightGun &dst, const picojson::value &src) {
        return true;
    }

    LightGun::LightGun() {
        this->hasLight = false;
    }
} // namespace ecs
