#pragma once

#include "Ecs.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/Controller.hh"
#include "ecs/components/Events.hh"
#include "ecs/components/Focus.hh"
#include "ecs/components/Gui.hh"
#include "ecs/components/Interact.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Mirror.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/SceneConnection.hh"
#include "ecs/components/SceneInfo.hh"
#include "ecs/components/Script.hh"
#include "ecs/components/Signals.hh"
#include "ecs/components/Transform.h"
#include "ecs/components/Triggers.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelArea.hh"
#include "ecs/components/XRView.hh"

#include <Tecs.hh>

namespace ecs {
    template<typename T>
    Tecs::Entity EntityWith(Lock<Read<T>> lock, const T &value) {
        for (auto e : lock.template EntitiesWith<T>()) {
            if (e.template Has<T>(lock) && e.template Get<const T>(lock) == value) return e;
        }
        return Tecs::Entity();
    }
} // namespace ecs
