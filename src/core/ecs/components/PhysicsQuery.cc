#include "PhysicsQuery.hh"

#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<PhysicsQuery>::Load(const EntityScope &scope, PhysicsQuery &query, const picojson::value &src) {
        return true;
    }
} // namespace ecs
