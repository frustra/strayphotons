/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"

#include <array>

namespace ecs {
    enum class NetworkPolicy : uint8_t {
        None = 0, // No updates are sent.
        Static, // Updates are only sent on component creation.
        Strict, // All updates must be received and processed in order.
        Lazy, // Updates can be dropped as long as they remain in order.
    };

    struct NetworkSettings {
        NetworkPolicy policy = NetworkPolicy::None;

        explicit operator bool() const {
            return policy != NetworkPolicy::None;
        }

        bool operator==(const NetworkSettings &other) const = default;
    };

    static StructMetadata MetadataNetworkSettings(typeid(NetworkSettings),
        sizeof(NetworkSettings),
        "NetworkSettings",
        "",
        StructField::New(&NetworkSettings::policy));

    struct Network {
        std::array<NetworkSettings, ECS::GetComponentCount()> components;
    };

    static EntityComponent<Network> ComponentNetwork("network", "");
    template<>
    void EntityComponent<Network>::Apply(Network &dst, const Network &src, bool liveTarget);
    template<>
    bool StructMetadata::Load<Network>(Network &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<Network>(const EntityScope &scope,
        picojson::value &dst,
        const Network &src,
        const Network *def);
} // namespace ecs
