#include "PhysicsQuery.hh"

#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<PhysicsQuery>::Load(const EntityScope &scope, PhysicsQuery &query, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "raycast") {
                query.queries.emplace_back(PhysicsQuery::Raycast(param.second.get<double>()));
            }
        }
        return true;
    }
} // namespace ecs
