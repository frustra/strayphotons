/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Network.hh"

#include "assets/JsonHelpers.hh"
#include "common/Logging.hh"
#include "ecs/Components.hh"
#include "ecs/EcsImpl.hh"

#include <picojson.h>

namespace ecs {

    namespace detail {
        template<typename... AllComponentTypes, template<typename...> typename ECSType, typename Func>
        void ForEachComponentType(ECSType<AllComponentTypes...> *, Func &&callback) {
            ( // For each component:
                [&] {
                    using T = AllComponentTypes;

                    if constexpr (std::is_same_v<T, ecs::Name> || std::is_same_v<T, ecs::SceneInfo>) {
                        // Skip
                    } else if constexpr (!Tecs::is_global_component<T>()) {
                        callback((T *)nullptr);
                    }
                }(),
                ...);
        }
    } // namespace detail

    // Calls the provided auto-lambda for all components except Name and SceneInfo
    template<typename Func>
    void ForEachComponentType(Func &&callback) {
        detail::ForEachComponentType((ecs::ECS *)nullptr, std::forward<Func>(callback));
    }

    template<>
    void EntityComponent<Network>::Apply(Network &dst, const Network &src, bool liveTarget) {
        if (liveTarget) {
            dst.components = src.components;
        } else {
            for (size_t i = 0; i < dst.components.size(); i++) {
                if (src.components[i] && !dst.components[i]) {
                    dst.components[i] = src.components[i];
                }
            }
        }
    }

    template<>
    bool StructMetadata::Load<Network>(Network &dst, const picojson::value &src) {
        auto &obj = src.get<picojson::object>();
        bool success = true;
        ForEachComponentType([&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            auto &comp = ecs::LookupComponent<T>();
            auto it = obj.find(comp.name);
            if (it != obj.end()) {
                const picojson::value &component = it->second;
                if (component.is<std::string>()) {
                    NetworkSettings settings = {};
                    if (sp::json::Load(settings, component)) {
                        dst.components[ECS::GetComponentIndex<T>()] = settings;
                    } else {
                        Errorf("Couldn't parse NetworkSettings value: %s", component.to_str());
                        success = false;
                    }
                } else {
                    Errorf("Unknown network settings value: %s", component.to_str());
                    success = false;
                }
            }
        });
        return success;
    }

    template<>
    void StructMetadata::Save<Network>(const EntityScope &scope,
        picojson::value &dst,
        const Network &src,
        const Network *def) {
        ForEachComponentType([&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            auto &comp = ecs::LookupComponent<T>();
            sp::json::SaveIfChanged(scope,
                dst,
                comp.name,
                src.components[ECS::GetComponentIndex<T>()],
                def ? &def->components[ECS::GetComponentIndex<T>()] : nullptr);
        });
    }
} // namespace ecs
