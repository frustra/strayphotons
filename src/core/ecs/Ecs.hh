#pragma once

#include <Tecs.hh>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <typeindex>

namespace picojson {
    class value;
}

namespace ecs {
    typedef std::string Name;

    class Animation;
    class HumanController;
    struct InteractController;
    struct EventInput;
    class EventBindings;
    struct Light;
    class LightSensor;
    struct Mirror;
    struct Owner;
    struct Physics;
    struct PhysicsState;
    struct Renderable;
    class Script;
    class SignalOutput;
    class SignalBindings;
    class Transform;
    struct TriggerArea;
    struct Triggerable;
    struct View;
    struct VoxelArea;
    struct XRView;

    using ECS = Tecs::ECS<Name,
                          Animation,
                          HumanController,
                          InteractController,
                          EventInput,
                          EventBindings,
                          Light,
                          LightSensor,
                          Mirror,
                          Owner,
                          Physics,
                          PhysicsState,
                          Renderable,
                          Script,
                          SignalOutput,
                          SignalBindings,
                          Transform,
                          TriggerArea,
                          Triggerable,
                          View,
                          VoxelArea,
                          XRView>;

    template<typename... Permissions>
    using Lock = Tecs::Lock<ECS, Permissions...>;
    template<typename... Components>
    using Read = Tecs::Read<Components...>;
    using ReadAll = Tecs::ReadAll;
    template<typename... Components>
    using Write = Tecs::Write<Components...>;
    using WriteAll = Tecs::WriteAll;
    using AddRemove = Tecs::AddRemove;
    template<typename Event>
    using Observer = Tecs::Observer<ECS, Event>;
    using EntityAdded = Tecs::EntityAdded;
    using EntityRemoved = Tecs::EntityRemoved;
    template<typename T>
    using Added = Tecs::Added<T>;
    template<typename T>
    using Removed = Tecs::Removed<T>;

    std::string ToString(Lock<Read<Name>> lock, Tecs::Entity e);

    extern ECS World;

    template<typename T>
    Tecs::Entity EntityWith(Lock<Read<T>> lock, const T &value);

    template<typename T>
    void DestroyAllWith(Lock<AddRemove> lock, const T &value);

}; // namespace ecs

static inline std::ostream &operator<<(std::ostream &out, const Tecs::Entity &v) {
    if (v) {
        return out << "Entity(" << v.id << ")";
    } else {
        return out << "Entity(void)";
    }
}
