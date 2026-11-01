/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/StructMetadata.hh"

#include <string>
using namespace std::string_literals;

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
        virtual bool HasComponent(const Lock<> &lock, Entity ent) const = 0;
        virtual const void *Access(const DynamicLock<> &lock, Entity ent) const = 0;
        virtual void *AccessMut(const DynamicLock<> &lock, Entity ent) const = 0;
        virtual const void *GetLiveDefault() const = 0;
        virtual const void *GetStagingDefault() const = 0;

        std::string name;
        StructMetadata metadata;
    };

    void RegisterComponent(const char *name, const std::type_index &idx, ComponentBase *comp);
    const ComponentBase *LookupComponent(std::string_view name);
    const ComponentBase *LookupComponent(const std::type_index &idx);

    template<typename T, std::enable_if_t<Tecs::is_global_component<T>::value, int> = 0>
    const GlobalComponent<T> &LookupComponent() {
        static const auto *ptr = dynamic_cast<const GlobalComponent<T> *>(LookupComponent(std::type_index(typeid(T))));
        Assertf(ptr != nullptr, "Couldn't lookup component type: %s", typeid(T).name());
        return *ptr;
    }

    template<typename T, std::enable_if_t<!Tecs::is_global_component<T>::value, int> = 0>
    const EntityComponent<T> &LookupComponent() {
        static const auto *ptr = dynamic_cast<const EntityComponent<T> *>(LookupComponent(std::type_index(typeid(T))));
        Assertf(ptr != nullptr, "Couldn't lookup component type: %s", typeid(T).name());
        return *ptr;
    }

    template<typename CompType>
    void ApplyComponent(CompType &dst, const CompType &src, bool liveTarget);

    template<typename CompType>
    class EntityComponent final : public ComponentBase {
    public:
        EntityComponent(StructMetadata &&metadata, const char *name) : ComponentBase(name, std::move(metadata)) {
            auto existing = dynamic_cast<const EntityComponent<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, std::type_index(typeid(CompType)), this);
            } else if (*this != *existing) {
                Abortf("Duplicate component type registered: %s", name);
            }
        }

        EntityComponent(StructMetadata &&metadata) : EntityComponent(std::move(metadata), metadata.name.c_str()) {}

        template<typename... Fields>
        EntityComponent(const char *name, const char *desc, Fields &&...fields)
            : EntityComponent({typeid(CompType), sizeof(CompType), name, desc, std::forward<Fields>(fields)...}) {}

        bool IsGlobal() const override {
            return false;
        }

        bool LoadFields(CompType &dst, const picojson::value &src) const;
        bool LoadEntity(FlatEntity &dst, const picojson::value &src) const override;
        void SaveEntity(const Lock<ReadAll> &lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const override;

        bool HasComponent(const Lock<> &lock, Entity ent) const override;
        const void *Access(const DynamicLock<> &lock, Entity ent) const override;
        void *AccessMut(const DynamicLock<> &lock, Entity ent) const override;

        const void *GetLiveDefault() const override;
        const void *GetStagingDefault() const override;
        static const CompType &LiveDefault();
        static const CompType &StagingDefault();

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

        template<typename T>
        friend void ApplyComponent(T &, const T &, bool);
    };

    template<typename CompType>
    class GlobalComponent final : public ComponentBase {
    public:
        GlobalComponent(StructMetadata &&metadata, const char *name) : ComponentBase(name, std::move(metadata)) {
            auto existing = dynamic_cast<const GlobalComponent<CompType> *>(LookupComponent(name));
            if (existing == nullptr) {
                RegisterComponent(name, std::type_index(typeid(CompType)), this);
            } else if (*this != *existing) {
                Abortf("Duplicate component type registered: %s", name);
            }
        }

        GlobalComponent(StructMetadata &&metadata) : GlobalComponent(std::move(metadata), metadata.name.c_str()) {}

        template<typename... Fields>
        GlobalComponent(const char *name, const char *desc, Fields &&...fields)
            : GlobalComponent({typeid(CompType), sizeof(CompType), name, desc, std::forward<Fields>(fields)...}) {}

        bool IsGlobal() const override {
            return true;
        }

        bool LoadFields(CompType &dst, const picojson::value &src) const;
        bool LoadEntity(FlatEntity &, const picojson::value &) const override {
            return false;
        }
        void SaveEntity(const Lock<ReadAll> &, const EntityScope &, picojson::value &, const Entity &) const override {}

        bool HasComponent(const Lock<> &lock, Entity) const override;
        const void *Access(const DynamicLock<> &lock, Entity) const override;
        void *AccessMut(const DynamicLock<> &lock, Entity) const override;

        const void *GetLiveDefault() const override;
        const void *GetStagingDefault() const override;
        static const CompType &LiveDefault();
        static const CompType &StagingDefault();

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

        template<typename T>
        friend void ApplyComponent(T &, const T &, bool);
    };
}; // namespace ecs
