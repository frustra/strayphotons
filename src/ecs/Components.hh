#pragma once

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace picojson {
    class value;
}

namespace ecs {
    class Entity;

    class ComponentBase {
    public:
        ComponentBase(const char *name) : name(name) {}

        virtual bool LoadEntity(Entity &dst, picojson::value &src) = 0;
        virtual bool SaveEntity(picojson::value &dst, Entity &src) = 0;

        const char *name;
    };

    void RegisterComponent(const char *name, ComponentBase *comp);
    ComponentBase *LookupComponent(const std::string name);

    template<typename CompType>
    class Component : public ComponentBase {
    public:
        Component(const char *name) : ComponentBase(name) {
            auto existing = dynamic_cast<Component<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, this);
            } else if (*this != *existing) {
                throw std::runtime_error("Duplicate component type registered: " + std::string(name));
            }
        }

        bool LoadEntity(Entity &dst, picojson::value &src) override {
            std::cerr << "Calling undefined LoadEntity on type: " << name << std::endl;
            return false;
        }

        bool SaveEntity(picojson::value &dst, Entity &src) override {
            std::cerr << "Calling undefined SaveEntity on type: " << name << std::endl;
            return false;
        }

        bool operator==(const Component<CompType> &other) const {
            return strcmp(name, other.name) == 0;
        }

        bool operator!=(const Component<CompType> &other) const {
            return !(*this == other);
        }
    };
}; // namespace ecs
