#pragma once

#include <cstring>
#include <ecs/Ecs.hh>
#include <iostream>
#include <stdexcept>

namespace picojson {
    class value;
}

namespace ecs {
    class ComponentBase {
    public:
        ComponentBase(const char *name) : name(name) {}

        virtual bool LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) = 0;
        virtual bool SaveEntity(Lock<ReadAll> lock, picojson::value &dst, const Tecs::Entity &src) = 0;

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

        bool LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) override {
            auto &comp = dst.Set<CompType>(lock);
            return Load(lock, comp, src);
        }

        bool SaveEntity(Lock<ReadAll> lock, picojson::value &dst, const Tecs::Entity &src) override {
            auto &comp = src.Get<CompType>(lock);
            return Save(lock, dst, comp);
        }

        static bool Load(Lock<Read<ecs::Name>> lock, CompType &dst, const picojson::value &src) {
            std::cerr << "Calling undefined Load(json) on Compoent type: " << typeid(CompType).name() << std::endl;
            return false;
        }

        static bool Save(Lock<Read<ecs::Name>> lock, picojson::value &dst, const CompType &src) {
            std::cerr << "Calling undefined Save(json) on Compoent type: " << typeid(CompType).name() << std::endl;
            return false;
        }

        static bool Load(Lock<Read<ecs::Name>> lock, CompType &dst /*, const protobuf &src*/) {
            std::cerr << "Calling undefined Load(protobuf) on Compoent type: " << typeid(CompType).name() << std::endl;
            return false;
        }

        static bool Save(Lock<Read<ecs::Name>> lock /*, protobuf &dst*/, const CompType &src) {
            std::cerr << "Calling undefined Save(protobuf) on Compoent type: " << typeid(CompType).name() << std::endl;
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
