#pragma once

#include "ecs/Ecs.hh"
#include "ecs/StructMetadata.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"

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

    template<typename>
    class Component;

    class ComponentBase {
    public:
        ComponentBase(const char *name, const StructMetadata &metadata) : name(name), metadata(metadata) {}

        virtual bool LoadEntity(Lock<AddRemove> lock,
            const EntityScope &scope,
            Entity &dst,
            const picojson::value &src) const = 0;
        virtual void SaveEntity(Lock<ReadAll> lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const = 0;
        virtual void ApplyComponent(Lock<ReadAll> src, Entity srcEnt, Lock<AddRemove> dst, Entity dstEnt) const = 0;
        virtual bool HasComponent(Lock<> lock, Entity ent) const = 0;
        virtual const void *Access(Lock<ReadAll> lock, Entity ent) const = 0;
        virtual void *Access(Lock<WriteAll> lock, Entity ent) const = 0;

        template<typename T>
        void ApplyComponent(const T &src, Lock<AddRemove> dstLock, Entity dst) const {
            dynamic_cast<const Component<T> *>(this)->ApplyComponent(src, dstLock, dst);
        }

        const char *name;
        const StructMetadata &metadata;
    };

    void RegisterComponent(const char *name, const std::type_index &idx, ComponentBase *comp);
    const ComponentBase *LookupComponent(const std::string &name);
    const ComponentBase *LookupComponent(const std::type_index &idx);
    void ForEachComponent(std::function<void(const std::string &, const ComponentBase &)> callback);

    template<typename T>
    const Component<T> &LookupComponent() {
        auto ptr = LookupComponent(std::type_index(typeid(T)));
        Assertf(ptr != nullptr, "Couldn't lookup component type: %s", typeid(T).name());
        return *dynamic_cast<const Component<T> *>(ptr);
    }

    template<typename CompType>
    class Component : public ComponentBase {
    private:
        const CompType defaultLiveComponent = {};
        const CompType defaultStagingComponent;

        CompType makeDefaultStagingComponent(const StructMetadata &metadata) {
            static const CompType defaultComp = {};
            CompType comp = {};
            for (auto &field : metadata.fields) {
                field.InitUndefined(&comp, &defaultComp);
            }
            StructMetadata::InitUndefined(comp);
            return comp;
        }

    public:
        Component(const char *name, const StructMetadata &metadata)
            : ComponentBase(name, metadata), defaultStagingComponent(makeDefaultStagingComponent(metadata)) {
            auto existing = dynamic_cast<const Component<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, std::type_index(typeid(CompType)), this);
            } else if (*this != *existing) {
                throw std::runtime_error("Duplicate component type registered: " + std::string(name));
            }
        }

        bool LoadFields(const EntityScope &scope, CompType &dst, const picojson::value &src) const {
            for (auto &field : metadata.fields) {
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
            DebugAssert(IsStaging(lock), "LoadEntity should only be called with a staging lock");
            if (dst.Has<CompType>(lock)) {
                CompType srcComp = defaultStagingComponent;
                if (!LoadFields(scope, srcComp, src)) return false;
                if (!StructMetadata::Load<CompType>(scope, srcComp, src)) return false;
                auto &comp = dst.Get<CompType>(lock);
                for (auto &field : metadata.fields) {
                    field.Apply(&comp, &srcComp, &defaultStagingComponent);
                }
                Apply(srcComp, lock, dst);
                return true;
            } else {
                auto &comp = dst.Set<CompType>(lock, defaultStagingComponent);
                if (!LoadFields(scope, comp, src)) return false;
                return StructMetadata::Load<CompType>(scope, comp, src);
            }
        }

        void SaveEntity(Lock<ReadAll> lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const override {
            auto &comp = src.Get<CompType>(lock);

            if (IsLive(lock)) {
                for (auto &field : metadata.fields) {
                    field.Save(scope, dst, &comp, &defaultLiveComponent);
                }
            } else {
                for (auto &field : metadata.fields) {
                    field.Save(scope, dst, &comp, &defaultStagingComponent);
                }
            }
            StructMetadata::Save<CompType>(scope, dst, comp);
        }

        void ApplyComponent(const CompType &src, Lock<AddRemove> dstLock, Entity dst) const {
            const auto &defaultComponent = IsLive(dstLock) ? defaultLiveComponent : defaultStagingComponent;
            CompType *dstComp;
            if (!dst.Has<CompType>(dstLock)) {
                dstComp = &dst.Set<CompType>(dstLock, defaultComponent);
            } else {
                dstComp = &dst.Get<CompType>(dstLock);
            }
            // Merge existing component with a new one
            for (auto &field : metadata.fields) {
                field.Apply(dstComp, &src, &defaultComponent);
            }
            Apply(src, dstLock, dst);
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

        const void *Access(Lock<ReadAll> lock, Entity ent) const override {
            return &ent.Get<CompType>(lock);
        }

        void *Access(Lock<WriteAll> lock, Entity ent) const override {
            return &ent.Get<CompType>(lock);
        }

        bool operator==(const Component<CompType> &other) const {
            return strcmp(name, other.name) == 0;
        }

        bool operator!=(const Component<CompType> &other) const {
            return !(*this == other);
        }

    protected:
        static void Apply(const CompType &src, Lock<AddRemove> lock, Entity dst) {
            // Custom field apply is always called, default to no-op.
        }
    };
}; // namespace ecs
