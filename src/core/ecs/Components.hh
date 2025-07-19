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

    class ComponentBase : public sp::NonMoveable {
    public:
        ComponentBase(const char *name, StructMetadata &&metadata) : name(name), metadata(std::move(metadata)) {}

        virtual bool IsGlobal() const = 0;
        virtual bool LoadEntity(FlatEntity &dst, const picojson::value &src) const = 0;
        virtual void SaveEntity(const Lock<ReadAll> &lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const = 0;
        virtual void SetComponent(const Lock<AddRemove> &lock, const EntityScope &scope, const Entity &dst) const = 0;
        virtual void SetComponent(const Lock<AddRemove> &lock,
            const EntityScope &scope,
            const Entity &dst,
            const FlatEntity &src) const = 0;
        virtual void UnsetComponent(const Lock<AddRemove> &lock, const Entity &dst) const = 0;
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

        std::string name;
        StructMetadata metadata;
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

        CompType makeDefaultStagingComponent(const StructMetadata &metadata);

    public:
        Component(StructMetadata &&metadata, const char *name);
        Component(StructMetadata &&metadata) : Component(std::move(metadata), metadata.name.c_str()) {}

        bool IsGlobal() const override {
            return Tecs::is_global_component<CompType>();
        }

        bool LoadFields(CompType &dst, const picojson::value &src) const;
        bool LoadEntity(FlatEntity &dst, const picojson::value &src) const override;
        void SaveEntity(const Lock<ReadAll> &lock,
            const EntityScope &scope,
            picojson::value &dst,
            const Entity &src) const override;

        void ApplyComponent(CompType &dst, const CompType &src, bool liveTarget) const;
        void SetComponent(const Lock<AddRemove> &lock, const EntityScope &scope, const Entity &dst) const override;
        void SetComponent(const Lock<AddRemove> &lock,
            const EntityScope &scope,
            const Entity &dst,
            const FlatEntity &src) const override;
        void UnsetComponent(const Lock<AddRemove> &lock, const Entity &dst) const override;
        bool HasComponent(const Lock<> &lock, Entity ent) const override;
        bool HasComponent(const FlatEntity &ent) const override;

        const void *Access(const Lock<ReadAll> &lock, Entity ent) const override;
        void *Access(const Lock<WriteAll> &lock, Entity ent) const override;
        void *Access(const PhysicsWriteLock &lock, Entity ent) const override;

        const void *GetLiveDefault() const override;
        const void *GetStagingDefault() const override;
        const CompType &StagingDefault() const;

        bool operator==(const Component<CompType> &other) const;
        bool operator!=(const Component<CompType> &other) const;

    protected:
        static void Apply(CompType &dst, const CompType &src, bool liveTarget) {
            // Custom field apply is always called, default to no-op.
        }
    };
}; // namespace ecs
