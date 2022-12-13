#pragma once

#include "core/Tracing.hh"

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
    struct Name;
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
    struct OpticalElement;
    struct Physics;
    struct PhysicsJoints;
    struct PhysicsQuery;
    struct Renderable;
    struct SceneConnection;
    struct SceneInfo;
    struct Screen;
    struct Script;
    struct SignalOutput;
    struct SignalBindings;
    struct Sounds;
    struct Transform;
    typedef Transform TransformSnapshot;
    struct TransformTree;
    struct TriggerArea;
    enum class TriggerGroup : uint8_t;
    struct View;
    struct VoxelArea;
    struct XRView;

    using ECS = Tecs::ECS<Name,
        SceneInfo,
        TransformSnapshot,
        TransformTree,
        Renderable,
        Physics,

        Animation,
        CharacterController,
        FocusLayer,
        FocusLock,
        Gui,
        LaserEmitter,
        LaserLine,
        LaserSensor,
        Light,
        LightSensor,
        OpticalElement,
        PhysicsJoints,
        PhysicsQuery,
        SceneConnection,
        Screen,
        Sounds,
        TriggerArea,
        TriggerGroup,
        View,
        VoxelArea,
        XRView,

        EventInput,
        EventBindings,
        SignalOutput,
        SignalBindings,
        Script>;

    using Entity = Tecs::Entity;
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

    std::string ToString(Lock<Read<Name>> lock, Entity e);

    ECS &World();
    ECS &StagingWorld();

    int GetComponentIndex(const std::string &componentName);

    template<typename... Permissions>
    inline auto StartTransaction() {
        return World().StartTransaction<Permissions...>();
    }

    template<typename... Permissions>
    inline auto StartStagingTransaction() {
        return StagingWorld().StartTransaction<Permissions...>();
    }

    static inline bool IsLive(const Entity &e) {
        return Tecs::IdentifierFromGeneration(e.generation) == World().GetInstanceId();
    }

    static inline bool IsLive(Lock<> lock) {
        return lock.GetInstance().GetInstanceId() == World().GetInstanceId();
    }

    static inline bool IsStaging(const Entity &e) {
        return Tecs::IdentifierFromGeneration(e.generation) == StagingWorld().GetInstanceId();
    }

    static inline bool IsStaging(Lock<> lock) {
        return lock.GetInstance().GetInstanceId() == StagingWorld().GetInstanceId();
    }
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
TECS_NAME_COMPONENT(ecs::OpticalElement, "OpticalElement");
TECS_NAME_COMPONENT(ecs::Physics, "Physics");
TECS_NAME_COMPONENT(ecs::PhysicsJoints, "PhysicsJoints");
TECS_NAME_COMPONENT(ecs::PhysicsQuery, "PhysicsQuery");
TECS_NAME_COMPONENT(ecs::Renderable, "Renderable");
TECS_NAME_COMPONENT(ecs::SceneConnection, "SceneConnection");
TECS_NAME_COMPONENT(ecs::SceneInfo, "SceneInfo");
TECS_NAME_COMPONENT(ecs::Screen, "Screen");
TECS_NAME_COMPONENT(ecs::Script, "Script");
TECS_NAME_COMPONENT(ecs::SignalOutput, "SignalOutput");
TECS_NAME_COMPONENT(ecs::SignalBindings, "SignalBindings");
TECS_NAME_COMPONENT(ecs::Sounds, "Sound");
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
