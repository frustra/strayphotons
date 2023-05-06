#pragma once

#include "core/Common.hh"
#include "core/EntityMap.hh"
#include "core/LockFreeMutex.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/Signals.hh"

#include <atomic>
#include <memory>
#include <robin_hood.h>

namespace ecs {
    class ReferenceManager {
    public:
        ReferenceManager() {}

        EntityRef GetEntity(const Name &name);
        EntityRef GetEntity(const Entity &entity);
        EntityRef SetEntity(const Name &name, const Entity &entity);
        std::set<Name> GetEntityNames(const std::string &search = "");

        SignalRef GetSignal(const SignalKey &signal);
        std::set<SignalKey> GetSignals(const std::string &search = "");

        void Tick(chrono_clock::duration maxTickInterval);

    private:
        sp::LockFreeMutex entityMutex;
        sp::PreservingMap<Name, EntityRef::Ref, 1000> entityRefs;
        sp::EntityMap<std::weak_ptr<EntityRef::Ref>> stagingRefs;
        sp::EntityMap<std::weak_ptr<EntityRef::Ref>> liveRefs;

        sp::LockFreeMutex signalMutex;
        sp::PreservingMap<SignalKey, SignalKey, 1000> signalRefs;
    };

    struct EntityRef::Ref {
        ecs::Name name;
        std::atomic<Entity> stagingEntity;
        std::atomic<Entity> liveEntity;

        Ref(const ecs::Name &name) : name(name) {}
        Ref(const Entity &ent);
    };

    ReferenceManager &GetRefManager();
} // namespace ecs
