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
    void DestroyAllWith(Lock<AddRemove> lock, const T &value) {
        for (auto e : lock.template EntitiesWith<T>()) {
            if (e.template Get<T>(lock) == value) { e.Destroy(lock); }
        }
    }

    template<typename T>
    Tecs::Entity EntityWith(Lock<Read<T>> lock, const T &value) {
        for (auto e : lock.template EntitiesWith<T>()) {
            if (e.template Get<T>(lock) == value) return e;
        }
        return Tecs::Entity();
    }
} // namespace ecs