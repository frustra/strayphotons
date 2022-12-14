#include "Components.hh"

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

    void ForEachComponent(std::function<void(const std::string &, const ComponentBase &)> callback) {
        Assertf(componentTypeMap != nullptr, "ForEachComponent called before components registered.");
        for (auto &[name, comp] : *componentNameMap) {
            if (name == "name" || name == "scene_info") continue;
            callback(name, *comp);
        }
    }
} // namespace ecs
