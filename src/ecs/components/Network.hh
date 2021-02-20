#pragma once

#include "Physics.hh"
#include "Renderable.hh"
#include "Transform.hh"

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <variant>
#include <vector>

namespace ecs {
    // Placeholder policies for now, these will evolve over time.
    enum NetworkPolicy {
        NETWORK_POLICY_NONE = 0, // No updates are sent.
        NETWORK_POLICY_STATIC, // Updates are only sent on component creation.
        NETWORK_POLICY_STRICT, // All updates must be received and processed in order.
        NETWORK_POLICY_LAZY, // Updates can be dropped as long as they remain in order.
    };

    using ReadNetworkCompoenents = ecs::Read<ecs::Name, ecs::Network, ecs::Renderable, ecs::Transform, ecs::Physics>;

    struct NetworkedComponent {
        NetworkedComponent(ComponentBase *component = nullptr, NetworkPolicy policy = NETWORK_POLICY_NONE)
            : component(component), policy(policy) {}

        ComponentBase *component = nullptr;
        NetworkPolicy policy;
        bool initialized = false;
        std::variant<std::monostate, ecs::Renderable, ecs::Transform, ecs::Physics> lastUpdate;
    };

    struct Network {
        std::vector<NetworkedComponent> components;
    };

    static Component<Network> ComponentNetwork("network");

    template<>
    bool Component<Network>::Load(Lock<Read<ecs::Name>> lock, Network &dst, const picojson::value &src);
} // namespace ecs
