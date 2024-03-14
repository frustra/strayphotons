/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EntityReferenceManager.hh"

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalRef.hh"

#include <mutex>
#include <shared_mutex>

namespace ecs {
    EntityReferenceManager &GetEntityRefs() {
        return GetECSContext().refManager;
    }

    EntityRef EntityReferenceManager::Get(const Name &name) {
        if (!name) return EntityRef();

        EntityRef ref = entityRefs.Load(name);
        if (!ref) {
            std::lock_guard lock(mutex);
            ref = entityRefs.Load(name);
            if (ref) return ref;

            ref = make_shared<EntityRef::Ref>(name);
            entityRefs.Register(name, ref.ptr);
        }
        return ref;
    }

    EntityRef EntityReferenceManager::Get(const Entity &entity) {
        if (!entity) return EntityRef();

        std::shared_lock lock(mutex);
        if (IsLive(entity)) {
            if (liveRefs.count(entity) == 0) return EntityRef();
            return EntityRef(liveRefs[entity].lock());
        } else if (IsStaging(entity)) {
            if (stagingRefs.count(entity) == 0) return EntityRef();
            return EntityRef(stagingRefs[entity].lock());
        } else {
            Abortf("Invalid EntityReferenceManager entity: %s", std::to_string(entity));
        }
    }

    EntityRef EntityReferenceManager::Set(const Name &name, const Entity &entity) {
        Assertf(entity, "Trying to set EntityRef with null Entity");

        auto ref = Get(name);
        std::lock_guard lock(mutex);
        if (IsLive(entity)) {
            ref.ptr->liveEntity = entity;
            liveRefs[entity] = ref.ptr;
        } else if (IsStaging(entity)) {
            ref.ptr->stagingEntity = entity;
            stagingRefs[entity] = ref.ptr;
        } else {
            Abortf("Invalid EntityReferenceManager entity: %s", std::to_string(entity));
        }
        return ref;
    }

    std::set<Name> EntityReferenceManager::GetNames(const std::string &search) {
        std::set<Name> results;
        entityRefs.ForEach([&](auto &name, auto &) {
            if (search.empty() || name.String().find(search) != std::string::npos) {
                results.emplace(name);
            }
        });
        return results;
    }

    void EntityReferenceManager::Tick(chrono_clock::duration maxTickInterval) {
        entityRefs.Tick(maxTickInterval, [this](std::shared_ptr<EntityRef::Ref> &refPtr) {
            EntityRef ref(refPtr);
            auto staging = ref.GetStaging();
            auto live = ref.GetLive();
            if (staging || live) {
                std::lock_guard lock(mutex);
                if (staging) stagingRefs.erase(staging);
                if (live) liveRefs.erase(live);
            }
        });
    }
} // namespace ecs
