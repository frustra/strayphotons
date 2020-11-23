#pragma once

#include <cstring>
#include <ecs/Ecs.hh>
#include <iostream>
#include <stdexcept>

namespace picojson {
    class value;
}

namespace Tecs {
    struct Entity;
}

namespace ecs {
    class ComponentBase {
    protected:
        ComponentBase(const char *name) : name(name) {}

    public:
        virtual bool LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) = 0;
        virtual bool SaveEntity(Lock<AddRemove> lock, picojson::value &dst, const Tecs::Entity &src) = 0;

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

        bool Load(CompType &dst, const picojson::value &src) {
            std::cerr << "Calling undefined Load on type: " << name << std::endl;
            return false;
        }

        bool Save(picojson::value &dst, const CompType &src) {
            std::cerr << "Calling undefined Save on type: " << name << std::endl;
            return false;
        }

        bool LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) override {
            auto &comp = dst.Set<CompType>(lock);
            return Load(comp, src);
        }

        bool SaveEntity(Lock<AddRemove> lock, picojson::value &dst, const Tecs::Entity &src) override {
            const auto &comp = src.Get<CompType>(lock);
            return Save(dst, comp);
        }

        bool operator==(const Component<CompType> &other) const {
            return strcmp(name, other.name) == 0;
        }

        bool operator!=(const Component<CompType> &other) const {
            return !(*this == other);
        }
    };
}; // namespace ecs
