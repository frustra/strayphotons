#pragma once

#include "ecs/ComponentMetadata.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/Scene.hh"

#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <typeindex>

namespace picojson {
    class value;
}

namespace sp {
    class Scene;
} // namespace sp

namespace ecs {
    struct EntityScope {
        std::weak_ptr<sp::Scene> scene;
        Name prefix;
    };

    class ComponentBase {
    public:
        ComponentBase(const char *name) : name(name) {}

        virtual bool LoadEntity(Lock<AddRemove> lock,
            const EntityScope &scope,
            Entity &dst,
            const picojson::value &src) const = 0;
        virtual bool SaveEntity(Lock<ReadAll> lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const = 0;
        virtual void ApplyComponent(Lock<ReadAll> src, Entity srcEnt, Lock<AddRemove> dst, Entity dstEnt) const = 0;
        virtual bool HasComponent(Lock<> lock, Entity ent) const = 0;

        const char *name;
        std::vector<ComponentField> fields;
    };

    void RegisterComponent(const char *name, const std::type_index &idx, ComponentBase *comp);
    const ComponentBase *LookupComponent(const std::string &name);
    const ComponentBase *LookupComponent(const std::type_index &idx);
    void ForEachComponent(std::function<void(const std::string &, const ComponentBase &)> callback);

    template<typename CompType>
    class Component : public ComponentBase {
    public:
        Component() : ComponentBase("") {}
        Component(const char *name) : ComponentBase(name) {
            auto existing = dynamic_cast<const Component<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, std::type_index(typeid(CompType)), this);
            } else if (*this != *existing) {
                throw std::runtime_error("Duplicate component type registered: " + std::string(name));
            }
        }

        template<typename... Fields>
        Component(const char *name, Fields &&...fields) : Component(name) {
            (this->fields.emplace_back(fields), ...);
        }

        bool LoadFields(const EntityScope &scope, CompType &dst, const picojson::value &src) const {
            for (auto &field : this->fields) {
                if (!field.Load(scope, &dst, src)) {
                    Errorf("Component %s has invalid field: %s", name, field.name);
                    return false;
                }
            }
            return true;
        }

        bool LoadEntity(Lock<AddRemove> lock,
            const EntityScope &scope,
            Entity &dst,
            const picojson::value &src) const override {
            if (dst.Has<CompType>(lock)) {
                static const CompType defaultComp = {};
                CompType srcComp;
                if (!LoadFields(scope, srcComp, src)) return false;
                if (!Load(scope, srcComp, src)) return false;
                auto &comp = dst.Get<CompType>(lock);
                for (auto &field : this->fields) {
                    field.Apply(&comp, &srcComp, &defaultComp);
                }
                Apply(srcComp, lock, dst);
                return true;
            } else {
                auto &comp = dst.Set<CompType>(lock);
                if (!LoadFields(scope, comp, src)) return false;
                return Load(scope, comp, src);
            }
        }

        bool SaveEntity(Lock<ReadAll> lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const override {
            static const CompType defaultValue = {};
            auto &comp = src.Get<CompType>(lock);

            for (auto &field : this->fields) {
                field.SaveIfChanged(scope, dst, &comp, &defaultValue);
            }
            return Save(lock, scope, dst, comp);
        }

        void ApplyComponent(const CompType &src, Lock<AddRemove> dstLock, Entity dst) const {
            static const CompType defaultValues = {};
            if (!dst.Has<CompType>(dstLock)) {
                dst.Set<CompType>(dstLock, src);
            } else {
                // Merge existing component with a new one
                auto &dstComp = dst.Get<CompType>(dstLock);
                for (auto &field : this->fields) {
                    field.Apply(&dstComp, &src, &defaultValues);
                }
                Apply(src, dstLock, dst);
            }
        }

        void ApplyComponent(Lock<ReadAll> srcLock, Entity src, Lock<AddRemove> dstLock, Entity dst) const override {
            if (src.Has<CompType>(srcLock)) {
                auto &srcComp = src.Get<CompType>(srcLock);
                ApplyComponent(srcComp, dstLock, dst);
            }
        }

        bool HasComponent(Lock<> lock, Entity ent) const override {
            return ent.Has<CompType>(lock);
        }

        bool operator==(const Component<CompType> &other) const {
            return strcmp(name, other.name) == 0;
        }

        bool operator!=(const Component<CompType> &other) const {
            return !(*this == other);
        }

    protected:
        static bool Load(const EntityScope &scope, CompType &dst, const picojson::value &src) {
            // Custom field serialization is always called, default to no-op.
            return true;
        }

        static bool Save(Lock<Read<Name>> lock, const EntityScope &scope, picojson::value &dst, const CompType &src) {
            // Custom field serialization is always called, default to no-op.
            return true;
        }

        static void Apply(const CompType &src, Lock<AddRemove> lock, Entity dst) {}
    };

    template<typename T>
    const Component<T> &LookupComponent() {
        auto ptr = LookupComponent(std::type_index(typeid(T)));
        Assertf(ptr != nullptr, "Couldn't lookup component type: %s", typeid(T).name());
        return *dynamic_cast<const Component<T> *>(ptr);
    }
}; // namespace ecs
