#include "Components.hh"

#include <map>
#include <stdexcept>

namespace ecs {
    typedef std::map<std::string, ComponentBase *> ComponentList;
    ComponentList *GComponentList;

    void RegisterComponent(const char *name, ComponentBase *comp) {
        if (GComponentList == nullptr) GComponentList = new ComponentList();
        if (GComponentList->count(name) > 0)
            throw std::runtime_error("Duplicate component registration: " + std::string(name));
        GComponentList->emplace(name, comp);
    }

    ComponentBase *LookupComponent(const std::string name) {
        if (GComponentList == nullptr) GComponentList = new ComponentList();

        try {
            return GComponentList->at(name);
        } catch (std::out_of_range &) { return nullptr; }
    }
} // namespace ecs
