/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Components.hh"

#include "ecs/EcsImpl.hh"
#include "strayphotons/Hashing.hh"

#include <glm/glm.hpp>
#include <map>
#include <typeindex>

namespace ecs {
    typedef std::map<std::string, ComponentBase *, sp::StringLess> ComponentNameMap;
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

    const ComponentBase *LookupComponent(std::string_view name) {
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
} // namespace ecs
