/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EntityRef.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <atomic>

namespace ecs {
    NamedEntity::NamedEntity(Entity ent) : ent(ent) {
        name = GetEntityRefs().Get(ent).Name();
    }

    NamedEntity::NamedEntity(const ecs::Name &name, Entity ent) : name(name), ent(ent) {
        if (!ent) ent = GetEntityRefs().Get(name).GetLive();
    }

    ecs::Name NamedEntity::Name() const {
        if (ent && !name) {
            return GetEntityRefs().Get(ent).Name();
        } else {
            return name;
        }
    }

    Entity NamedEntity::Get(const Lock<> &lock) const {
        if (IsLive(lock) == IsLive(ent) && ent.Exists(lock)) {
            return ent;
        } else {
            return GetEntityRefs().Get(name).Get(lock);
        }
    }

    Entity NamedEntity::GetLive() const {
        if (IsLive(ent)) {
            return ent;
        } else {
            return GetEntityRefs().Get(name).GetLive();
        }
    }

    Entity NamedEntity::GetStaging() const {
        if (IsStaging(ent)) {
            return ent;
        } else {
            return GetEntityRefs().Get(name).GetStaging();
        }
    }

    bool NamedEntity::IsValid() const {
        return name || ent;
    }

    void NamedEntity::SetScope(const EntityScope &scope) {
        Assertf(name, "NamedEntity::SetScope called on empty name");
        ecs::Name newName(name, scope);
        if (!newName) {
            Clear();
        } else if (newName != name) {
            name = newName;
            ent = GetEntityRefs().Get(name).GetLive();
        }
    }

    void NamedEntity::Clear() {
        name = ecs::Name();
        ent = Entity();
    }

    NamedEntity NamedEntity::Lookup(Entity ent) {
        return NamedEntity(ent);
    }

    NamedEntity NamedEntity::Find(const char *name, const EntityScope *scope) {
        return NamedEntity(ecs::Name(name, scope ? *scope : EntityScope{}));
    }

    NamedEntity::operator EntityRef() const {
        if (name) {
            return EntityRef(name);
        } else {
            return EntityRef(ent);
        }
    }

    bool NamedEntity::operator==(const NamedEntity &other) const {
        return name == other.name;
    }

    bool NamedEntity::operator==(const Entity &other) const {
        return ent == other;
    }

    bool NamedEntity::operator<(const NamedEntity &other) const {
        return name < other.name;
    }

    EntityRef::EntityRef(const Entity &ent) {
        if (!ent) return;
        ptr = GetEntityRefs().Get(ent).ptr;
    }

    EntityRef::EntityRef(const ecs::Name &name, const Entity &ent) {
        if (!name) return;
        if (ent) {
            ptr = GetEntityRefs().Set(name, ent).ptr;
        } else {
            ptr = GetEntityRefs().Get(name).ptr;
        }
        Assertf(ptr, "EntityRef(%s, %s) is invalid", name.String(), std::to_string(ent));
    }

    ecs::Name EntityRef::Name() const {
        return ptr ? ptr->name : ecs::Name();
    }

    Entity EntityRef::Get(const ecs::Lock<> &lock) const {
        if (IsLive(lock)) {
            return GetLive();
        } else if (IsStaging(lock)) {
            return GetStaging();
        } else {
            Abortf("Invalid EntityRef lock: %u", lock.GetInstance().GetInstanceId());
        }
    }

    Entity EntityRef::GetLive() const {
        return ptr ? ptr->liveEntity.load() : Entity();
    }

    Entity EntityRef::GetStaging() const {
        return ptr ? ptr->stagingEntity.load() : Entity();
    }

    bool EntityRef::IsValid() const {
        return !!ptr;
    }

    void EntityRef::SetScope(const EntityScope &scope) {
        if (!ptr) return;
        ecs::Name newName(ptr->name, scope);
        if (!newName) {
            ptr = nullptr;
        } else if (newName != ptr->name) {
            ptr = GetEntityRefs().Get(newName).ptr;
        }
    }

    void EntityRef::Clear() {
        ptr.reset();
    }

    EntityRef EntityRef::Empty() {
        return EntityRef();
    }

    EntityRef EntityRef::New(Entity ent) {
        return EntityRef(ent);
    }

    EntityRef EntityRef::Copy(const EntityRef &ref) {
        return EntityRef(ref);
    }

    EntityRef EntityRef::Lookup(const char *name, const EntityScope *scope) {
        return EntityRef(ecs::Name(name, scope ? *scope : EntityScope{}));
    }

    bool EntityRef::operator==(const EntityRef &other) const {
        if (!ptr || !other.ptr) return ptr == other.ptr;
        if (ptr == other.ptr) return true;
        auto live = ptr->liveEntity.load();
        auto staging = ptr->stagingEntity.load();
        return (live && live == other.ptr->liveEntity.load()) ||
               (staging && staging == other.ptr->stagingEntity.load());
    }

    bool EntityRef::operator==(const NamedEntity &other) const {
        if (!ptr || !other) return false;
        return Name() == other.name;
    }

    bool EntityRef::operator==(const Entity &other) const {
        if (!ptr || !other) return false;
        return ptr->liveEntity.load() == other || ptr->stagingEntity.load() == other;
    }

    bool EntityRef::operator<(const EntityRef &other) const {
        return Name() < other.Name();
    }
} // namespace ecs
