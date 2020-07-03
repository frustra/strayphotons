#include "Network.hh"

#include <Common.hh>
#include <assets/AssetHelpers.hh>
#include <ecs/Components.hh>
#include <picojson/picojson.h>

namespace ecs {
	template<>
	bool Component<Network>::LoadEntity(Entity &dst, picojson::value &src) {
		auto network = dst.Assign<Network>();
		for (auto subNetwork : src.get<picojson::object>()) {
			if (subNetwork.first == "owner") {
				network->owner = subNetwork.second.get<UserId>();
			} else if (subNetwork.first == "policy") {
				std::string policy = sp::to_lower_copy(subNetwork.second.get<std::string>());
				if (policy == "strict") {
					network->policy = NETWORK_POLICY_STRICT;
				} else if (policy == "lazy") {
					network->policy = NETWORK_POLICY_LAZY;
				} else {
					network->policy = NETWORK_POLICY_NONE;
				}
			} else if (subNetwork.first == "components") {
				network->components.clear();
				if (subNetwork.second.is<picojson::array>()) {
					picojson::array &subSecond = subNetwork.second.get<picojson::array>();
					for (picojson::value &comp : subSecond) { network->components.push_back(comp.get<std::string>()); }
				} else {
					network->components.push_back(subNetwork.second.get<std::string>());
				}
			}
		}
		return true;
	}
} // namespace ecs
