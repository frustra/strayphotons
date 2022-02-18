#pragma once

#include "core/Tracing.hh"

#define ZoneScopedNOverrideFunction(varname, name, func)                            \
    static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location, \
        __LINE__){name, func, __FILE__, (uint32_t)__LINE__, 0};                     \
    tracy::ScopedZone varname(&TracyConcat(__tracy_source_location, __LINE__), true);

#define TECS_EXTERNAL_TRACE_TRANSACTION_STARTING(permissions) \
    ZoneScopedNOverrideFunction(___tracy_scoped_zone, "TransactionStart", permissions)
#define TECS_EXTERNAL_TRACE_TRANSACTION_ENDING(permissions) \
    ZoneScopedNOverrideFunction(___tracy_scoped_zone, "TransactionEnd", permissions)

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
    struct CharacterController;
    struct EventInput;
    class EventBindings;
    enum class FocusLayer : uint8_t;
    class FocusLock;
    struct Gui;
    struct LaserEmitter;
    struct LaserLine;
    struct LaserSensor;
    struct Light;
    class LightSensor;
    struct Mirror;
    struct Physics;
    struct PhysicsQuery;
    struct Renderable;
    struct SceneConnection;
    struct SceneInfo;
    struct Screen;
    class Script;
    class SignalOutput;
    class SignalBindings;
    class Sound;
    struct Transform;
    typedef Transform TransformSnapshot;
    struct TransformTree;
    struct TriggerArea;
    enum class TriggerGroup : uint8_t;
    struct View;
    struct VoxelArea;
    struct XRView;

    using ECS = Tecs::ECS<Name,
        Animation,
        CharacterController,
        EventInput,
        EventBindings,
        FocusLayer,
        FocusLock,
        Gui,
        LaserEmitter,
        LaserLine,
        LaserSensor,
        Light,
        LightSensor,
        Mirror,
        Physics,
        PhysicsQuery,
        Renderable,
        SceneConnection,
        SceneInfo,
        Screen,
        Script,
        SignalOutput,
        SignalBindings,
        Sound,
        TransformSnapshot,
        TransformTree,
        TriggerArea,
        TriggerGroup,
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
    template<typename T>
    using EntityObserver = Tecs::Observer<ECS, Tecs::EntityEvent>;
    using EntityEvent = Tecs::EntityEvent;
    template<typename T>
    using ComponentObserver = Tecs::Observer<ECS, Tecs::ComponentEvent<T>>;
    template<typename T>
    using ComponentEvent = Tecs::ComponentEvent<T>;

    std::string ToString(Lock<Read<Name>> lock, Tecs::Entity e);

    extern ECS World;

    template<typename T>
    Tecs::Entity EntityWith(Lock<Read<T>> lock, const T &value);

}; // namespace ecs

TECS_NAME_COMPONENT(ecs::Name, "Name");
TECS_NAME_COMPONENT(ecs::Animation, "Animation");
TECS_NAME_COMPONENT(ecs::CharacterController, "CharacterController");
TECS_NAME_COMPONENT(ecs::EventInput, "EventInput");
TECS_NAME_COMPONENT(ecs::EventBindings, "EventBindings");
TECS_NAME_COMPONENT(ecs::FocusLayer, "FocusLayer");
TECS_NAME_COMPONENT(ecs::FocusLock, "FocusLock");
TECS_NAME_COMPONENT(ecs::Gui, "Gui");
TECS_NAME_COMPONENT(ecs::LaserEmitter, "LaserEmitter");
TECS_NAME_COMPONENT(ecs::LaserLine, "LaserLine");
TECS_NAME_COMPONENT(ecs::LaserSensor, "LaserSensor");
TECS_NAME_COMPONENT(ecs::Light, "Light");
TECS_NAME_COMPONENT(ecs::LightSensor, "LightSensor");
TECS_NAME_COMPONENT(ecs::Mirror, "Mirror");
TECS_NAME_COMPONENT(ecs::Physics, "Physics");
TECS_NAME_COMPONENT(ecs::PhysicsQuery, "PhysicsQuery");
TECS_NAME_COMPONENT(ecs::Renderable, "Renderable");
TECS_NAME_COMPONENT(ecs::SceneConnection, "SceneConnection");
TECS_NAME_COMPONENT(ecs::SceneInfo, "SceneInfo");
TECS_NAME_COMPONENT(ecs::Screen, "Screen");
TECS_NAME_COMPONENT(ecs::Script, "Script");
TECS_NAME_COMPONENT(ecs::SignalOutput, "SignalOutput");
TECS_NAME_COMPONENT(ecs::SignalBindings, "SignalBindings");
TECS_NAME_COMPONENT(ecs::Sound, "Sound");
TECS_NAME_COMPONENT(ecs::TransformSnapshot, "TransformSnapshot");
TECS_NAME_COMPONENT(ecs::TransformTree, "TransformTree");
TECS_NAME_COMPONENT(ecs::TriggerArea, "TriggerArea");
TECS_NAME_COMPONENT(ecs::TriggerGroup, "TriggerGroup");
TECS_NAME_COMPONENT(ecs::View, "View");
TECS_NAME_COMPONENT(ecs::VoxelArea, "VoxelArea");
TECS_NAME_COMPONENT(ecs::XRView, "XRView");

static inline std::ostream &operator<<(std::ostream &out, const Tecs::Entity &e) {
    return out << std::to_string(e);
}
