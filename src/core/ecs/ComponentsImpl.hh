/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Components.hh"
#include "Tecs_permissions.hh"
#include "ecs/StructMetadata.hh"

namespace ecs {
    namespace detail {
        template<typename T>
        T makeDefaultStagingComponent(const StructMetadata &metadata) {
            static const T defaultComp = {};
            T comp = {};
            for (auto &field : metadata.fields) {
                field.InitUndefined(&comp, &defaultComp);
            }
            StructMetadata::InitUndefined(comp);
            return comp;
        }
    } // namespace detail

    template<typename CompType>
    void ApplyComponent(CompType &dst, const CompType &src, bool liveTarget) {
        static const StructMetadata &metadata = LookupComponent<CompType>().metadata;
        const void *defaultComponent;
        if constexpr (Tecs::is_global_component<CompType>()) {
            defaultComponent = liveTarget ? &GlobalComponent<CompType>::LiveDefault()
                                          : &GlobalComponent<CompType>::StagingDefault();
        } else {
            defaultComponent = liveTarget ? &EntityComponent<CompType>::LiveDefault()
                                          : &EntityComponent<CompType>::StagingDefault();
        }
        // Merge existing component with a new one
        for (auto &field : metadata.fields) {
            field.Apply(&dst, &src, defaultComponent);
        }
        if constexpr (Tecs::is_global_component<CompType>()) {
            GlobalComponent<CompType>::Apply(dst, src, liveTarget);
        } else {
            EntityComponent<CompType>::Apply(dst, src, liveTarget);
        }
    }

    template<typename CompType>
    bool EntityComponent<CompType>::LoadFields(CompType &dst, const picojson::value &src) const {
        for (auto &field : metadata.fields) {
            if (!field.Load(&dst, src)) {
                Errorf("Component %s has invalid field: %s", name, field.name);
                return false;
            }
        }
        return true;
    }

    template<typename CompType>
    bool EntityComponent<CompType>::LoadEntity(FlatEntity &dst, const picojson::value &src) const {
        CompType comp = StagingDefault();
        if (!LoadFields(comp, src)) return false;
        if (StructMetadata::Load<CompType>(comp, src)) {
            std::get<std::optional<CompType>>(dst) = std::move(comp);
            return true;
        }
        return false;
    }

    template<typename CompType>
    void EntityComponent<CompType>::SaveEntity(const Lock<ReadAll> &lock,
        const EntityScope &scope,
        picojson::value &dst,
        const Entity &src) const {
        auto &comp = src.Get<CompType>(lock);

        if (IsLive(lock)) {
            for (auto &field : metadata.fields) {
                field.Save(scope, dst, &comp, &LiveDefault());
            }
            StructMetadata::Save<CompType>(scope, dst, comp, &LiveDefault());
        } else {
            for (auto &field : metadata.fields) {
                field.Save(scope, dst, &comp, &StagingDefault());
            }
            StructMetadata::Save<CompType>(scope, dst, comp, &StagingDefault());
        }
    }

    template<typename CompType>
    bool EntityComponent<CompType>::HasComponent(const Lock<> &lock, Entity ent) const {
        return ent.Has<CompType>(lock);
    }

    template<typename CompType>
    const void *EntityComponent<CompType>::Access(const DynamicLock<> &lock, Entity ent) const {
        if (auto tryLock = lock.TryLock<Read<CompType>>()) {
            return &ent.Get<const CompType>(*tryLock);
        } else {
            return nullptr;
        }
    }

    template<typename CompType>
    void *EntityComponent<CompType>::AccessMut(const DynamicLock<> &lock, Entity ent) const {
        if (auto tryLock = lock.TryLock<Write<CompType>>()) {
            return &ent.Get<CompType>(*tryLock);
        } else {
            return nullptr;
        }
    }

    template<typename CompType>
    const void *EntityComponent<CompType>::GetLiveDefault() const {
        return &LiveDefault();
    }

    template<typename CompType>
    const void *EntityComponent<CompType>::GetStagingDefault() const {
        return &StagingDefault();
    }

    template<typename CompType>
    const CompType &EntityComponent<CompType>::LiveDefault() {
        static const CompType defaultLiveComponent = {};
        return defaultLiveComponent;
    }

    template<typename CompType>
    const CompType &EntityComponent<CompType>::StagingDefault() {
        static const CompType defaultStagingComponent = detail::makeDefaultStagingComponent<CompType>(
            LookupComponent<CompType>().metadata);
        return defaultStagingComponent;
    }

    template<typename CompType>
    bool GlobalComponent<CompType>::LoadFields(CompType &dst, const picojson::value &src) const {
        for (auto &field : metadata.fields) {
            if (!field.Load(&dst, src)) {
                Errorf("Component %s has invalid field: %s", name, field.name);
                return false;
            }
        }
        return true;
    }

    template<typename CompType>
    bool GlobalComponent<CompType>::HasComponent(const Lock<> &lock, Entity) const {
        return lock.Has<CompType>();
    }

    template<typename CompType>
    const void *GlobalComponent<CompType>::Access(const DynamicLock<> &lock, Entity ent) const {
        if (auto tryLock = lock.TryLock<Read<CompType>>()) {
            return &tryLock->template Get<const CompType>();
        } else {
            return nullptr;
        }
    }

    template<typename CompType>
    void *GlobalComponent<CompType>::AccessMut(const DynamicLock<> &lock, Entity ent) const {
        if (auto tryLock = lock.TryLock<Write<CompType>>()) {
            return &tryLock->template Get<CompType>();
        } else {
            return nullptr;
        }
    }

    template<typename CompType>
    const void *GlobalComponent<CompType>::GetLiveDefault() const {
        return &LiveDefault();
    }

    template<typename CompType>
    const void *GlobalComponent<CompType>::GetStagingDefault() const {
        return &StagingDefault();
    }

    template<typename CompType>
    const CompType &GlobalComponent<CompType>::LiveDefault() {
        static const CompType defaultLiveComponent = {};
        return defaultLiveComponent;
    }

    template<typename CompType>
    const CompType &GlobalComponent<CompType>::StagingDefault() {
        static const CompType defaultStagingComponent = detail::makeDefaultStagingComponent<CompType>(
            LookupComponent<CompType>().metadata);
        return defaultStagingComponent;
    }
}; // namespace ecs
