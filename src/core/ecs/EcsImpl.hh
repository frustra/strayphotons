#pragma once

#include "Ecs.hh"

#include <Tecs.hh>
#include <ecs/components/Animation.hh>
#include <ecs/components/Controller.hh>
#include <ecs/components/Events.hh>
#include <ecs/components/Interact.hh>
#include <ecs/components/Light.hh>
#include <ecs/components/LightSensor.hh>
#include <ecs/components/Mirror.hh>
#include <ecs/components/Owner.hh>
#include <ecs/components/Physics.hh>
#include <ecs/components/Renderable.hh>
#include <ecs/components/Script.hh>
#include <ecs/components/Signals.hh>
#include <ecs/components/Transform.hh>
#include <ecs/components/TriggerArea.hh>
#include <ecs/components/Triggerable.hh>
#include <ecs/components/View.hh>
#include <ecs/components/VoxelArea.hh>
#include <ecs/components/XRView.hh>

namespace ecs {
    template<typename T>
    T &Handle<T>::operator*() {
        auto lock = ecs.StartTransaction<Tecs::Write<T>>();
        return e.Get<T>(lock);
    }

    template<typename T>
    T *Handle<T>::operator->() {
        auto lock = ecs.StartTransaction<Tecs::Write<T>>();
        return &e.Get<T>(lock);
    }

    template<typename T, typename... Args>
    Handle<T> Entity::Assign(Args... args) {
        auto lock = em->tecs.StartTransaction<Tecs::AddRemove>();
        e.Set<T>(lock, args...);
        return Handle<T>(em->tecs, e);
    }

    template<typename T>
    bool Entity::Has() {
        auto lock = em->tecs.StartTransaction<Tecs::Read<T>>();
        return e.Has<T>(lock);
    }

    template<typename T>
    Handle<T> Entity::Get() {
        return Handle<T>(em->tecs, e);
    }

    template<typename T>
    void EntityManager::DestroyAllWith(const T &value) {
        auto lock = tecs.StartTransaction<Tecs::AddRemove>();
        for (auto e : lock.template EntitiesWith<T>()) {
            if (e.template Get<T>(lock) == value) { e.Destroy(lock); }
        }
    }

    template<typename T, typename... Tn>
    std::vector<Entity> EntityManager::EntitiesWith() {
        auto lock = tecs.StartTransaction<Tecs::Read<T, Tn...>>();

        auto &entities = lock.template EntitiesWith<T>();
        std::vector<Entity> result;
        for (auto e : entities) {
            if (!e.template Has<T, Tn...>(lock)) continue;
            result.emplace_back(this, e);
        }
        return result;
    }

    template<typename T>
    Entity EntityManager::EntityWith(const T &value) {
        auto lock = tecs.template StartTransaction<Tecs::Read<T>>();

        for (auto e : lock.template EntitiesWith<T>()) {
            if (e.template Get<T>(lock) == value) return Entity(this, e);
        }
        return Entity();
    }

    template<typename T>
    static Tecs::Entity EntityWith(Lock<Read<T>> lock, const T &value) {
        for (auto e : lock.template EntitiesWith<T>()) {
            if (e.template Get<T>(lock) == value) return e;
        }
        return Tecs::Entity();
    }
} // namespace ecs
