#pragma once

#include "core/DispatchQueue.hh"
#include "core/Tracing.hh"

#include <Tecs.hh>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <type_traits>
#include <typeindex>

namespace picojson {
    class value;
}

namespace ecs {
    struct Name;
    struct ActiveScene;
    class Animation;
    struct CharacterController;
    struct EventInput;
    class EventBindings;
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
    struct SceneProperties;
    struct Screen;
    struct Scripts;
    struct Signals;
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
        SceneProperties,
        TransformSnapshot,
        TransformTree,
        Renderable,
        Physics,

        ActiveScene,
        Animation,
        CharacterController,
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
        Signals,
        SignalOutput,
        SignalBindings,
        Scripts>;

    using Entity = Tecs::Entity;
    template<typename... Permissions>
    using Lock = Tecs::Lock<ECS, Permissions...>;
    template<typename... Permissions>
    using DynamicLock = Tecs::DynamicLock<ECS, Permissions...>;
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
    sp::DispatchQueue &TransactionQueue();

    int GetComponentIndex(const std::string &componentName);

    template<typename... Permissions>
    inline auto StartTransaction() {
        return World().StartTransaction<Permissions...>();
    }

    template<typename... Permissions>
    inline auto StartStagingTransaction() {
        return StagingWorld().StartTransaction<Permissions...>();
    }

    /**
     * Queues a transaction in a globally serialized queue. Ideal for non-blocking write transactions.
     *
     * Returns a future that will be resolved with the return value of the callback.
     * The function will be called from the ECSTransactionQueue thread with the acquired transaction lock.
     *
     * If the return value of the callback is itself a future, the future returned
     * from QueueTransaction will be set to its value once it is ready.
     *
     * Usage: QueueTransaction<Permissions...>([](auto lock) { return value; });
     * Example:
     *  AsyncPtr<Entity> ent = QueueTransaction<AddRemove>([](auto lock) { return lock.NewEntity(); });
     *  QueueTransaction<Write<FocusLock>>([ent](auto lock) { Assert(ent.Ready()); lock.Set<FocusLock>(); });
     */
    template<typename... Permissions, typename Fn>
    inline auto QueueTransaction(Fn &&callback)
        -> sp::AsyncPtr<std::invoke_result_t<Fn, const Lock<Permissions...> &>> {
        using ReturnType = std::invoke_result_t<Fn, const Lock<Permissions...> &>;
        return TransactionQueue().Dispatch<ReturnType>([callback = std::move(callback)]() {
            Lock<Permissions...> lock = World().StartTransaction<Permissions...>();
            if constexpr (std::is_void_v<ReturnType>) {
                callback(lock);
            } else {
                return std::make_shared<ReturnType>(callback(lock));
            }
        });
    }

    // See QueueTransaction() for usage.
    template<typename... Permissions, typename Fn>
    inline auto QueueStagingTransaction(Fn &&callback)
        -> sp::AsyncPtr<std::invoke_result_t<Fn, const Lock<Permissions...> &>> {
        using ReturnType = std::invoke_result_t<Fn, const Lock<Permissions...> &>;
        return TransactionQueue().Dispatch<ReturnType>([callback = std::move(callback)]() {
            Lock<Permissions...> lock = StagingWorld().StartTransaction<Permissions...>();
            if constexpr (std::is_void_v<ReturnType>) {
                callback(lock);
            } else {
                return std::make_shared<ReturnType>(callback(lock));
            }
        });
    }

    static inline bool IsLive(const Entity &e) {
        return Tecs::IdentifierFromGeneration(e.generation) == World().GetInstanceId();
    }

    static inline bool IsLive(const Lock<> &lock) {
        return lock.GetInstance().GetInstanceId() == World().GetInstanceId();
    }

    static inline bool IsStaging(const Entity &e) {
        return Tecs::IdentifierFromGeneration(e.generation) == StagingWorld().GetInstanceId();
    }

    static inline bool IsStaging(const Lock<> &lock) {
        return lock.GetInstance().GetInstanceId() == StagingWorld().GetInstanceId();
    }
}; // namespace ecs

TECS_NAME_COMPONENT(ecs::Name, "Name");
TECS_NAME_COMPONENT(ecs::ActiveScene, "ActiveScene");
TECS_NAME_COMPONENT(ecs::Animation, "Animation");
TECS_NAME_COMPONENT(ecs::CharacterController, "CharacterController");
TECS_NAME_COMPONENT(ecs::EventInput, "EventInput");
TECS_NAME_COMPONENT(ecs::EventBindings, "EventBindings");
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
TECS_NAME_COMPONENT(ecs::SceneProperties, "SceneProperties");
TECS_NAME_COMPONENT(ecs::Screen, "Screen");
TECS_NAME_COMPONENT(ecs::Scripts, "Scripts");
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
