#pragma once

#include <Common.hh>
#include <Ecs.hh>
#include <ecs/Components.hh>

namespace ecs {
	typedef uint64_t UserId;

	// Placeholder policies for now, these will evolve over time.
	enum NetworkPolicy {
		NETWORK_POLICY_NONE = 0, // No updates are sent.
		NETWORK_POLICY_STRICT,   // All updates must be received and processed in order.
		NETWORK_POLICY_LAZY,     // Updates can be dropped as long as they remain in order.
	};

	struct Network {
		UserId owner;
		NetworkPolicy policy;
		std::vector<std::string> components;
	};

	static Component<Network> ComponentNetwork("network");

	template<>
	bool Component<Network>::LoadEntity(Entity &dst, picojson::value &src);
} // namespace ecs
