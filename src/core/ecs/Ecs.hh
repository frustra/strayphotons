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
    struct CharacterController;
    struct EventInput;
    class EventBindings;
    enum class FocusLayer : uint8_t;
    class FocusLock;
    struct Gui;
    struct LaserLine;
    struct Light;
    class LightSensor;
    struct Mirror;
    struct Physics;
    struct PhysicsQuery;
    struct Renderable;
    struct SceneConnection;
    struct SceneInfo;
    class Script;
    class SignalOutput;
    class SignalBindings;
    struct Transform;
    struct TriggerArea;
    enum class TriggerGroup : uint8_t;
    struct View;
    struct VoxelArea;
    struct XRView;

    using ECS = Tecs::ECS<Name,
        Animation,
        HumanController,
        CharacterController,
        EventInput,
        EventBindings,
        FocusLayer,
        FocusLock,
        Gui,
        LaserLine,
        Light,
        LightSensor,
        Mirror,
        Physics,
        PhysicsQuery,
        Renderable,
        SceneConnection,
        SceneInfo,
        Script,
        SignalOutput,
        SignalBindings,
        Transform,
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
TECS_NAME_COMPONENT(ecs::HumanController, "HumanController");
TECS_NAME_COMPONENT(ecs::CharacterController, "CharacterController");
TECS_NAME_COMPONENT(ecs::EventInput, "EventInput");
TECS_NAME_COMPONENT(ecs::EventBindings, "EventBindings");
TECS_NAME_COMPONENT(ecs::FocusLayer, "FocusLayer");
TECS_NAME_COMPONENT(ecs::FocusLock, "FocusLock");
TECS_NAME_COMPONENT(ecs::Gui, "Gui");
TECS_NAME_COMPONENT(ecs::LaserLine, "LaserLine");
TECS_NAME_COMPONENT(ecs::Light, "Light");
TECS_NAME_COMPONENT(ecs::LightSensor, "LightSensor");
TECS_NAME_COMPONENT(ecs::Mirror, "Mirror");
TECS_NAME_COMPONENT(ecs::Physics, "Physics");
TECS_NAME_COMPONENT(ecs::PhysicsQuery, "PhysicsQuery");
TECS_NAME_COMPONENT(ecs::Renderable, "Renderable");
TECS_NAME_COMPONENT(ecs::SceneConnection, "SceneConnection");
TECS_NAME_COMPONENT(ecs::SceneInfo, "SceneInfo");
TECS_NAME_COMPONENT(ecs::Script, "Script");
TECS_NAME_COMPONENT(ecs::SignalOutput, "SignalOutput");
TECS_NAME_COMPONENT(ecs::SignalBindings, "SignalBindings");
TECS_NAME_COMPONENT(ecs::Transform, "Transform");
TECS_NAME_COMPONENT(ecs::TriggerArea, "TriggerArea");
TECS_NAME_COMPONENT(ecs::TriggerGroup, "TriggerGroup");
TECS_NAME_COMPONENT(ecs::View, "View");
TECS_NAME_COMPONENT(ecs::VoxelArea, "VoxelArea");
TECS_NAME_COMPONENT(ecs::XRView, "XRView");

static inline std::ostream &operator<<(std::ostream &out, const Tecs::Entity &e) {
    return out << std::to_string(e);
}
