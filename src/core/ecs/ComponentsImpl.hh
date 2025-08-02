/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Components.hh"

namespace ecs {
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
        CompType comp = defaultStagingComponent;
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

    template<typename CompType>
    void EntityComponent<CompType>::ApplyComponent(CompType &dst, const CompType &src, bool liveTarget) const {
        const auto &defaultComponent = liveTarget ? defaultLiveComponent : defaultStagingComponent;
        // Merge existing component with a new one
        for (auto &field : metadata.fields) {
            field.Apply(&dst, &src, &defaultComponent);
        }
        Apply(dst, src, liveTarget);
    }

    template<typename CompType>
    void EntityComponent<CompType>::SetComponent(const Lock<AddRemove> &lock,
        const EntityScope &scope,
        const Entity &dst) const {
        auto &comp = dst.Set<CompType>(lock);
        scope::SetScope(comp, scope);
    }

    template<typename CompType>
    void EntityComponent<CompType>::SetComponent(const Lock<AddRemove> &lock,
        const EntityScope &scope,
        const Entity &dst,
        const FlatEntity &src) const {
        auto &opt = std::get<std::optional<CompType>>(src);
        if (opt) {
            auto &comp = dst.Set<CompType>(lock, *opt);
            scope::SetScope(comp, scope);
        }
    }

    template<typename CompType>
    void EntityComponent<CompType>::UnsetComponent(const Lock<AddRemove> &lock, const Entity &dst) const {
        dst.Unset<CompType>(lock);
    }

    template<typename CompType>
    bool EntityComponent<CompType>::HasComponent(const Lock<> &lock, Entity ent) const {
        return ent.Has<CompType>(lock);
    }

    template<typename CompType>
    bool EntityComponent<CompType>::HasComponent(const FlatEntity &ent) const {
        return (bool)std::get<std::optional<CompType>>(ent);
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
        return &defaultLiveComponent;
    }

    template<typename CompType>
    const void *EntityComponent<CompType>::GetStagingDefault() const {
        return &defaultStagingComponent;
    }

    template<typename CompType>
    const CompType &EntityComponent<CompType>::StagingDefault() const {
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
    void GlobalComponent<CompType>::ApplyComponent(CompType &dst, const CompType &src, bool liveTarget) const {
        const auto &defaultComponent = liveTarget ? defaultLiveComponent : defaultStagingComponent;
        // Merge existing component with a new one
        for (auto &field : metadata.fields) {
            field.Apply(&dst, &src, &defaultComponent);
        }
        Apply(dst, src, liveTarget);
    }

    template<typename CompType>
    void GlobalComponent<CompType>::SetComponent(const Lock<AddRemove> &lock,
        const EntityScope &scope,
        const Entity &dst) const {
        auto &comp = lock.Set<CompType>();
        scope::SetScope(comp, scope);
    }

    template<typename CompType>
    void GlobalComponent<CompType>::SetComponent(const Lock<AddRemove> &lock,
        const EntityScope &scope,
        const Entity &dst,
        const FlatEntity &src) const {
        auto &opt = std::get<std::optional<CompType>>(src);
        if (opt) {
            auto &comp = lock.Set<CompType>(*opt);
            scope::SetScope(comp, scope);
        }
    }

    template<typename CompType>
    void GlobalComponent<CompType>::UnsetComponent(const Lock<AddRemove> &lock, const Entity &dst) const {
        lock.Unset<CompType>();
    }

    template<typename CompType>
    bool GlobalComponent<CompType>::HasComponent(const Lock<> &lock, Entity) const {
        return lock.Has<CompType>();
    }

    template<typename CompType>
    bool GlobalComponent<CompType>::HasComponent(const FlatEntity &ent) const {
        return (bool)std::get<std::optional<CompType>>(ent);
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
        return &defaultLiveComponent;
    }

    template<typename CompType>
    const void *GlobalComponent<CompType>::GetStagingDefault() const {
        return &defaultStagingComponent;
    }

    template<typename CompType>
    const CompType &GlobalComponent<CompType>::StagingDefault() const {
        return defaultStagingComponent;
    }
}; // namespace ecs
