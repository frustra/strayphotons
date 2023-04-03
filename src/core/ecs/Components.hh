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
    template<typename>
    class Component;

    using PhysicsWriteLock = Lock<
        Write<TransformTree, OpticalElement, Physics, PhysicsJoints, PhysicsQuery, SignalOutput, LaserLine, VoxelArea>>;

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
        virtual bool HasComponent(Lock<> lock, Entity ent) const = 0;
        virtual const void *Access(Lock<Optional<ReadAll>> lock, Entity ent) const = 0;
        virtual void *Access(Lock<Optional<WriteAll>> lock, Entity ent) const = 0;
        virtual const void *Access(EntityLock<Optional<ReadAll>> entLock) const = 0;
        virtual void *Access(EntityLock<Optional<WriteAll>> entLock) const = 0;
        virtual const void *GetLiveDefault() const = 0;
        virtual const void *GetStagingDefault() const = 0;

        template<typename T>
        void ApplyComponent(T &dst, const T &src, bool liveTarget) const {
            dynamic_cast<const Component<T> *>(this)->ApplyComponent(dst, src, liveTarget);
        }

        template<typename T>
        const T &GetStagingDefault() const {
            return *static_cast<const T *>(dynamic_cast<const Component<T> *>(this)->GetStagingDefault());
        }

        const char *name;
        const StructMetadata &metadata;
    };

    void RegisterComponent(const char *name, const std::type_index &idx, ComponentBase *comp);
    const ComponentBase *LookupComponent(const std::string &name);
    const ComponentBase *LookupComponent(const std::type_index &idx);

    // Calls the provided function for all components except Name and SceneInfo
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

            auto &dstComp = dst.Set<CompType>(lock, defaultStagingComponent);
            if (!LoadFields(scope, dstComp, src)) return false;
            return StructMetadata::Load<CompType>(scope, dstComp, src);
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
                StructMetadata::Save<CompType>(scope, dst, comp, defaultLiveComponent);
            } else {
                for (auto &field : metadata.fields) {
                    field.Save(scope, dst, &comp, &defaultStagingComponent);
                }
                StructMetadata::Save<CompType>(scope, dst, comp, defaultStagingComponent);
            }
        }

        void ApplyComponent(CompType &dst, const CompType &src, bool liveTarget) const {
            const auto &defaultComponent = liveTarget ? defaultLiveComponent : defaultStagingComponent;
            // Merge existing component with a new one
            for (auto &field : metadata.fields) {
                field.Apply(&dst, &src, &defaultComponent);
            }
            Apply(dst, src, liveTarget);
        }

        bool HasComponent(Lock<> lock, Entity ent) const override {
            return ent.Has<CompType>(lock);
        }

        const void *Access(Lock<Optional<ReadAll>> lock, Entity ent) const override {
            auto readLock = lock.TryLock<Read<CompType>>();
            if (readLock) {
                return &ent.Get<const CompType>(*readLock);
            } else {
                return nullptr;
            }
        }

        void *Access(Lock<Optional<WriteAll>> lock, Entity ent) const override {
            auto writeLock = lock.TryLock<Write<CompType>>();
            if (writeLock) {
                return &ent.Get<CompType>(*writeLock);
            } else {
                return nullptr;
            }
        }

        const void *Access(EntityLock<Optional<ReadAll>> entLock) const override {
            auto readLock = entLock.TryLock<Read<CompType>>();
            if (readLock) {
                return &readLock->Get<const CompType>();
            } else {
                return nullptr;
            }
        }

        void *Access(EntityLock<Optional<WriteAll>> entLock) const override {
            auto writeLock = entLock.TryLock<Write<CompType>>();
            if (writeLock) {
                return &writeLock->Get<CompType>();
            } else {
                return nullptr;
            }
        }

        const void *GetLiveDefault() const override {
            return &defaultLiveComponent;
        }

        const void *GetStagingDefault() const override {
            return &defaultStagingComponent;
        }

        const CompType &StagingDefault() const {
            return defaultStagingComponent;
        }

        bool operator==(const Component<CompType> &other) const {
            return strcmp(name, other.name) == 0;
        }

        bool operator!=(const Component<CompType> &other) const {
            return !(*this == other);
        }

    protected:
        static void Apply(CompType &dst, const CompType &src, bool liveTarget) {
            // Custom field apply is always called, default to no-op.
        }
    };

    // Define these special components here to solve circular includes
    static StructMetadata MetadataName(typeid(Name));
    static Component<Name> ComponentName("name", MetadataName);
    static StructMetadata MetadataSceneInfo(typeid(SceneInfo));
    static Component<SceneInfo> ComponentSceneInfo("scene_info", MetadataSceneInfo);
}; // namespace ecs
