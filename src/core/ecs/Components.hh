/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
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
    class EntityComponent;
    template<typename>
    class GlobalComponent;

    using PhysicsWriteLock = Lock<
        Write<TransformTree, OpticalElement, Physics, PhysicsJoints, PhysicsQuery, Signals, LaserLine, VoxelArea>>;

    class ComponentBase : public sp::NonMoveable {
    public:
        ComponentBase(const char *name, StructMetadata &&metadata) : name(name), metadata(std::move(metadata)) {}

        virtual bool IsGlobal() const = 0;
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
            if constexpr (Tecs::is_global_component<T>()) {
                dynamic_cast<const GlobalComponent<T> *>(this)->ApplyComponent(dst, src, liveTarget);
            } else {
                dynamic_cast<const EntityComponent<T> *>(this)->ApplyComponent(dst, src, liveTarget);
            }
        }

        template<typename T>
        const T &GetStagingDefault() const {
            if constexpr (Tecs::is_global_component<T>()) {
                return *static_cast<const T *>(dynamic_cast<const GlobalComponent<T> *>(this)->GetStagingDefault());
            } else {
                return *static_cast<const T *>(dynamic_cast<const EntityComponent<T> *>(this)->GetStagingDefault());
            }
        }

        std::string name;
        StructMetadata metadata;
    };

    void RegisterComponent(const char *name, const std::type_index &idx, ComponentBase *comp);
    const ComponentBase *LookupComponent(const std::string &name);
    const ComponentBase *LookupComponent(const std::type_index &idx);

    // Calls the provided function for all components except Name and SceneInfo
    void ForEachComponent(std::function<void(const std::string &, const ComponentBase &)> callback);

    template<typename T, std::enable_if_t<Tecs::is_global_component<T>::value, int> = 0>
    const GlobalComponent<T> &LookupComponent() {
        auto ptr = LookupComponent(std::type_index(typeid(T)));
        Assertf(ptr != nullptr, "Couldn't lookup component type: %s", typeid(T).name());
        return *dynamic_cast<const GlobalComponent<T> *>(ptr);
    }

    template<typename T, std::enable_if_t<!Tecs::is_global_component<T>::value, int> = 0>
    const EntityComponent<T> &LookupComponent() {
        auto ptr = LookupComponent(std::type_index(typeid(T)));
        Assertf(ptr != nullptr, "Couldn't lookup component type: %s", typeid(T).name());
        return *dynamic_cast<const EntityComponent<T> *>(ptr);
    }

    template<typename CompType>
    class EntityComponent : public ComponentBase {
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
        EntityComponent(StructMetadata &&metadata, const char *name)
            : ComponentBase(name, std::move(metadata)), defaultStagingComponent(makeDefaultStagingComponent(metadata)) {
            auto existing = dynamic_cast<const EntityComponent<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, std::type_index(typeid(CompType)), this);
            } else if (*this != *existing) {
                throw std::runtime_error("Duplicate component type registered: "s + name);
            }
        }

        EntityComponent(StructMetadata &&metadata) : EntityComponent(std::move(metadata), metadata.name.c_str()) {}

        template<typename... Fields>
        EntityComponent(const char *name, const char *desc, Fields &&...fields)
            : EntityComponent({typeid(CompType), sizeof(CompType), name, desc, std::forward<Fields>(fields)...}) {}

        bool IsGlobal() const override {
            return false;
        }

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

        bool operator==(const EntityComponent<CompType> &other) const {
            return name == other.name && metadata == other.metadata;
        }

        bool operator!=(const EntityComponent<CompType> &other) const {
            return !(*this == other);
        }

    protected:
        static void Apply(CompType &dst, const CompType &src, bool liveTarget) {
            // Custom field apply is always called, default to no-op.
        }
    };

    template<typename CompType>
    class GlobalComponent : public ComponentBase {
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
        GlobalComponent(StructMetadata &&metadata, const char *name)
            : ComponentBase(name, std::move(metadata)), defaultStagingComponent(makeDefaultStagingComponent(metadata)) {
            auto existing = dynamic_cast<const GlobalComponent<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, std::type_index(typeid(CompType)), this);
            } else if (*this != *existing) {
                throw std::runtime_error("Duplicate component type registered: "s + name);
            }
        }

        GlobalComponent(StructMetadata &&metadata) : GlobalComponent(std::move(metadata), metadata.name.c_str()) {}

        template<typename... Fields>
        GlobalComponent(const char *name, const char *desc, Fields &&...fields)
            : GlobalComponent({typeid(CompType), sizeof(CompType), name, desc, std::forward<Fields>(fields)...}) {}

        bool IsGlobal() const override {
            return true;
        }

        bool LoadFields(CompType &dst, const picojson::value &src) const {
            for (auto &field : metadata.fields) {
                if (!field.Load(&dst, src)) {
                    Errorf("Component %s has invalid field: %s", name, field.name);
                    return false;
                }
            }
            return true;
        }

        bool LoadEntity(FlatEntity &, const picojson::value &) const override {
            return false;
        }

        void SaveEntity(const Lock<ReadAll> &, const EntityScope &, picojson::value &, const Entity &) const override {}

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
            const Entity &,
            const FlatEntity &src) const override {
            auto &opt = std::get<std::shared_ptr<CompType>>(src);
            if (opt) {
                auto &comp = lock.Set<CompType>(*opt);
                scope::SetScope(comp, scope);
            }
        }

        bool HasComponent(const Lock<> &lock, Entity) const override {
            return lock.Has<CompType>();
        }

        bool HasComponent(const FlatEntity &ent) const override {
            return (bool)std::get<std::shared_ptr<CompType>>(ent);
        }

        const void *Access(const Lock<ReadAll> &lock, Entity) const override {
            return &lock.Get<CompType>();
        }

        void *Access(const Lock<WriteAll> &lock, Entity) const override {
            return &lock.Get<CompType>();
        }

        void *Access(const PhysicsWriteLock &lock, Entity) const override {
            if constexpr (Tecs::is_write_allowed<CompType, PhysicsWriteLock>()) {
                return &lock.Get<CompType>();
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

        bool operator==(const GlobalComponent<CompType> &other) const {
            return name == other.name && metadata == other.metadata;
        }

        bool operator!=(const GlobalComponent<CompType> &other) const {
            return !(*this == other);
        }

    protected:
        static void Apply(CompType &dst, const CompType &src, bool liveTarget) {
            // Custom field apply is always called, default to no-op.
        }
    };

    // Define these special components here to solve circular includes
    static EntityComponent<Name> ComponentName(
        StructMetadata{
            typeid(Name),
            sizeof(Name),
            "Name",
            DocsDescriptionName,
            StructField::New("scene", &Name::scene, FieldAction::None),
            StructField::New("entity", &Name::entity, FieldAction::None),
        },
        "name");
    static EntityComponent<SceneInfo> ComponentSceneInfo("SceneInfo",
        "This is an internal component storing each entity's source scene and other creation info.");

    static StructMetadata MetadataEntityRef(typeid(EntityRef),
        sizeof(EntityRef),
        "EntityRef",
        DocsDescriptionEntityRef);
}; // namespace ecs
