/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
        Write<TransformTree, OpticalElement, Physics, PhysicsJoints, PhysicsQuery, Signals, LaserLine, VoxelArea>>;

    class ComponentBase {
    public:
        ComponentBase(const char *name, const StructMetadata &metadata) : name(name), metadata(metadata) {}

        virtual bool LoadEntity(FlatEntity &dst, const picojson::value &src) const = 0;
        virtual void SaveEntity(const Lock<ReadAll> &lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const = 0;
        virtual void SetComponent(const Lock<AddRemove> &lock,
            const EntityScope &scope,
            const Entity &dst,
            const FlatEntity &src) const = 0;
        virtual bool HasComponent(const Lock<> &lock, Entity ent) const = 0;
        virtual bool HasComponent(const FlatEntity &ent) const = 0;
        virtual const void *Access(const Lock<ReadAll> &lock, Entity ent) const = 0;
        virtual void *Access(const Lock<WriteAll> &lock, Entity ent) const = 0;
        virtual void *Access(const PhysicsWriteLock &lock, Entity ent) const = 0;
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
        Component(const StructMetadata &metadata, const char *name)
            : ComponentBase(name, metadata), defaultStagingComponent(makeDefaultStagingComponent(metadata)) {
            auto existing = dynamic_cast<const Component<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, std::type_index(typeid(CompType)), this);
            } else if (*this != *existing) {
                throw std::runtime_error("Duplicate component type registered: "s + name);
            }
        }

        Component(const StructMetadata &metadata) : Component(metadata, metadata.name.c_str()) {}

        bool LoadFields(CompType &dst, const picojson::value &src) const {
            for (auto &field : metadata.fields) {
                if (!field.Load(&dst, src)) {
                    Errorf("Component %s has invalid field: %s", name, field.name);
                    return false;
                }
            }
            return true;
        }

        bool LoadEntity(FlatEntity &dst, const picojson::value &src) const override {
            CompType comp = defaultStagingComponent;
            if (!LoadFields(comp, src)) return false;
            if (StructMetadata::Load<CompType>(comp, src)) {
                std::get<std::shared_ptr<CompType>>(dst) = std::make_shared<CompType>(std::move(comp));
                return true;
            }
            return false;
        }

        void SaveEntity(const Lock<ReadAll> &lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const override {
            auto &comp = src.Get<CompType>(lock);

            if (IsLive(lock)) {
                for (auto &field : metadata.fields) {
                    field.Save(scope, dst, &comp, &defaultLiveComponent);
                }
                StructMetadata::Save<CompType>(scope, dst, comp, &defaultLiveComponent);
            } else {
                for (auto &field : metadata.fields) {
                    field.Save(scope, dst, &comp, &defaultStagingComponent);
                }
                StructMetadata::Save<CompType>(scope, dst, comp, &defaultStagingComponent);
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

        void SetComponent(const Lock<AddRemove> &lock,
            const EntityScope &scope,
            const Entity &dst,
            const FlatEntity &src) const override {
            auto &opt = std::get<std::shared_ptr<CompType>>(src);
            if (opt) {
                auto &comp = dst.Set<CompType>(lock, *opt);
                scope::SetScope(comp, scope);
            }
        }

        bool HasComponent(const Lock<> &lock, Entity ent) const override {
            return ent.Has<CompType>(lock);
        }

        bool HasComponent(const FlatEntity &ent) const override {
            return (bool)std::get<std::shared_ptr<CompType>>(ent);
        }

        const void *Access(const Lock<ReadAll> &lock, Entity ent) const override {
            return &ent.Get<CompType>(lock);
        }

        void *Access(const Lock<WriteAll> &lock, Entity ent) const override {
            return &ent.Get<CompType>(lock);
        }

        void *Access(const PhysicsWriteLock &lock, Entity ent) const override {
            if constexpr (Tecs::is_write_allowed<CompType, PhysicsWriteLock>()) {
                return &ent.Get<CompType>(lock);
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
    static StructMetadata MetadataName(typeid(Name), "Name", DocsDescriptionName);
    static Component<Name> ComponentName(MetadataName, "name");
    static StructMetadata MetadataSceneInfo(typeid(SceneInfo),
        "SceneInfo",
        "This is an internal component storing each entity's source scene and other creation info.");
    static Component<SceneInfo> ComponentSceneInfo(MetadataSceneInfo);

    static StructMetadata MetadataEntityRef(typeid(EntityRef), "EntityRef", DocsDescriptionEntityRef);
}; // namespace ecs
