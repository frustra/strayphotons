/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Components.hh"

#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <map>
#include <stdexcept>
#include <typeindex>

namespace ecs {
    typedef std::map<std::string, ComponentBase *> ComponentNameMap;
    typedef std::map<std::type_index, ComponentBase *> ComponentTypeMap;
    ComponentNameMap *componentNameMap = nullptr;
    ComponentTypeMap *componentTypeMap = nullptr;

    void RegisterComponent(const char *name, const std::type_index &idx, ComponentBase *comp) {
        if (componentNameMap == nullptr) componentNameMap = new ComponentNameMap();
        if (componentTypeMap == nullptr) componentTypeMap = new ComponentTypeMap();
        Assertf(componentNameMap->count(name) == 0, "Duplicate component name registration: %s", name);
        Assertf(componentTypeMap->count(idx) == 0, "Duplicate component type registration: %s", name);
        componentNameMap->emplace(name, comp);
        componentTypeMap->emplace(idx, comp);
    }

    const ComponentBase *LookupComponent(const std::string &name) {
        if (componentNameMap == nullptr) componentNameMap = new ComponentNameMap();

        auto it = componentNameMap->find(name);
        if (it != componentNameMap->end()) return it->second;
        return nullptr;
    }

    const ComponentBase *LookupComponent(const std::type_index &idx) {
        if (componentTypeMap == nullptr) componentTypeMap = new ComponentTypeMap();

        auto it = componentTypeMap->find(idx);
        if (it != componentTypeMap->end()) return it->second;
        return nullptr;
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void forEachComponent(ECSType<AllComponentTypes...> *,
        std::function<void(const std::string &, const ComponentBase &)> &callback) {
        ( // For each component:
            [&] {
                using T = AllComponentTypes;

                if constexpr (std::is_same_v<T, Name> || std::is_same_v<T, SceneInfo>) {
                    // Skip
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    Assertf(componentTypeMap != nullptr, "ForEachComponent called before components registered.");
                    auto base = LookupComponent(typeid(T));
                    Assertf(base != nullptr, "ForEachComponent component lookup returned nullptr.");
                    callback(base->name, *base);
                }
            }(),
            ...);
    }

    void ForEachComponent(std::function<void(const std::string &, const ComponentBase &)> callback) {
        forEachComponent((ecs::ECS *)nullptr, callback);
    }
} // namespace ecs
