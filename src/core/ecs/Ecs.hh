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
    struct Light;
    class LightSensor;
    struct Mirror;
    struct Owner;
    struct Physics;
    struct PhysicsState;
    struct Renderable;
    class Script;
    class SignalOutput;
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
                          Light,
                          LightSensor,
                          Mirror,
                          Owner,
                          Physics,
                          PhysicsState,
                          Renderable,
                          Script,
                          SignalOutput,
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

    class EntityManager;

    template<typename T>
    class Handle {
    public:
        Handle(ECS &ecs, Tecs::Entity &e) : ecs(ecs), e(e) {}

        T &operator*();
        T *operator->();

    private:
        ECS &ecs;
        Tecs::Entity e;
    };

    class Entity {
    public:
        Entity() : e(), em(nullptr) {}
        Entity(EntityManager *em, const Tecs::Entity &e) : e(e.id), em(em) {}

        using Id = decltype(Tecs::Entity::id);

        Tecs::Entity GetEntity() const {
            return e;
        }

        Id GetId() const {
            return e.id;
        }

        EntityManager *GetManager() {
            return em;
        }

        inline bool Valid() const {
            return !!e && em != nullptr;
        }

        inline operator bool() const {
            return !!e;
        }

        inline bool operator==(const Entity &other) const {
            return e == other.e && em == other.em;
        }

        inline bool operator!=(const Entity &other) const {
            return e != other.e || em != other.em;
        }

        template<typename T, typename... Args>
        Handle<T> Assign(Args... args);

        template<typename T>
        bool Has();

        template<typename T>
        Handle<T> Get();

        void Destroy();

        inline std::string ToString() const {
            if (this->Valid()) {
                return "Entity(" + std::to_string(e.id) + ")";
            } else {
                return "Entity(NULL)";
            }
        }

        friend std::ostream &operator<<(std::ostream &os, const Entity e) {
            if (e.Valid()) {
                os << "Entity(" << e.e.id << ")";
            } else {
                os << "Entity(NULL)";
            }
            return os;
        }

    private:
        Tecs::Entity e;
        EntityManager *em = nullptr;
    };

    class EntityManager {
    public:
        EntityManager();

        Entity NewEntity();
        void DestroyAll();
        template<typename T>
        void DestroyAllWith(const T &value);
        template<typename T, typename... Tn>
        std::vector<Entity> EntitiesWith();
        template<typename T>
        Entity EntityWith(const T &value);

        ECS tecs;

    private:
        friend class Entity;
    };
}; // namespace ecs
