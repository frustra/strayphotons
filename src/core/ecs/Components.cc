#include "Components.hh"

#include <glm/glm.hpp>
#include <map>
#include <stdexcept>
#include <typeindex>

namespace ecs {
    typedef std::map<std::string, ComponentBase *> ComponentNameMap;
    typedef std::map<std::type_index, ComponentBase *> ComponentTypeMap;
    ComponentNameMap *GComponentNameMap;
    ComponentTypeMap *GComponentTypeMap;

    void RegisterComponent(const char *name, const std::type_index &idx, ComponentBase *comp) {
        if (GComponentNameMap == nullptr) GComponentNameMap = new ComponentNameMap();
        if (GComponentTypeMap == nullptr) GComponentTypeMap = new ComponentTypeMap();
        Assertf(GComponentNameMap->count(name) == 0, "Duplicate component name registration: %s", name);
        Assertf(GComponentTypeMap->count(idx) == 0, "Duplicate component type registration: %s", name);
        GComponentNameMap->emplace(name, comp);
        GComponentTypeMap->emplace(idx, comp);
    }

    const ComponentBase *LookupComponent(const std::string &name) {
        if (GComponentNameMap == nullptr) GComponentNameMap = new ComponentNameMap();

        auto it = GComponentNameMap->find(name);
        if (it != GComponentNameMap->end()) return it->second;
        return nullptr;
    }

    const ComponentBase *LookupComponent(const std::type_index &idx) {
        if (GComponentTypeMap == nullptr) GComponentTypeMap = new ComponentTypeMap();

        auto it = GComponentTypeMap->find(idx);
        if (it != GComponentTypeMap->end()) return it->second;
        return nullptr;
    }
} // namespace ecs
