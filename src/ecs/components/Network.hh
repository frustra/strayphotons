#pragma once

#include <Common.hh>
#include <ecs/Components.hh>
#include <ecs/Ecs.hh>

namespace ecs {
    typedef uint64_t UserId;

    // Placeholder policies for now, these will evolve over time.
    enum NetworkPolicy {
        NETWORK_POLICY_NONE = 0, // No updates are sent.
        NETWORK_POLICY_STATIC, // Updates are only sent on component creation.
        NETWORK_POLICY_STRICT, // All updates must be received and processed in order.
        NETWORK_POLICY_LAZY, // Updates can be dropped as long as they remain in order.
    };

    struct Network {
        UserId owner;
        std::vector<std::tuple<std::string, NetworkPolicy>> components;
    };

    static Component<Network> ComponentNetwork("network");

    template<>
    bool Component<Network>::Load(Lock<Read<ecs::Name>> lock, Network &dst, const picojson::value &src);
} // namespace ecs
