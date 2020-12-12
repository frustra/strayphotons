#include "Network.hh"

#include <Common.hh>
#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Network>::Load(Lock<Read<ecs::Name>> lock, Network &network, const picojson::value &src) {
        for (auto subNetwork : src.get<picojson::object>()) {
            if (subNetwork.first == "owner") {
                network.owner = UserId(subNetwork.second.get<double>());
            } else if (subNetwork.first == "components") {
                network.components.clear();
                for (auto component : subNetwork.second.get<picojson::object>()) {
                    std::string policy = sp::to_lower_copy(component.second.get<std::string>());
                    if (policy == "static") {
                        network.components.emplace_back(component.first, NETWORK_POLICY_STATIC);
                    } else if (policy == "strict") {
                        network.components.emplace_back(component.first, NETWORK_POLICY_STRICT);
                    } else if (policy == "lazy") {
                        network.components.emplace_back(component.first, NETWORK_POLICY_LAZY);
                    } else {
                        network.components.emplace_back(component.first, NETWORK_POLICY_NONE);
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
