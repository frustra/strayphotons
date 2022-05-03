#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Focus.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <optional>
#include <queue>
#include <robin_hood.h>
#include <set>
#include <string>
#include <variant>

namespace ecs {
    static const size_t MAX_EVENT_BINDING_DEPTH = 5;

    struct Event {
        using EventData = std::
            variant<bool, char, int, double, glm::vec2, glm::vec3, Transform, EntityRef, Tecs::Entity, std::string>;

        std::string name;
        Entity source;
        EventData data;

        Event() {}
        Event(const std::string &name, const Entity &source) : name(name), source(source), data(true) {}

        template<typename T>
        Event(const std::string &name, const Entity &source, T data) : name(name), source(source), data(data) {}

        std::string toString() const;
    };

    using SendEventsLock = Lock<Read<Name, FocusLayer, FocusLock, EventBindings, EventInput>>;

    std::ostream &operator<<(std::ostream &out, const Event::EventData &v);

    class EventQueue {
    public:
        void Add(const Event &event);
        bool Empty();
        size_t Size();
        bool Poll(Event &eventOut);

    private:
        std::queue<Event> events;
        sp::LockFreeMutex mutex;
    };

    struct EventInput {
        robin_hood::unordered_map<std::string, shared_ptr<EventQueue>> events;

        EventInput() {}

        template<class... Args>
        EventInput(const Args &...eventList) {
            for (auto &event : {eventList...}) {
                Register(event);
            }
        }

        void Register(const std::string &binding);
        bool IsRegistered(const std::string &binding) const;
        void Unregister(const std::string &binding);

        bool Add(const std::string &binding, const Event &event) const;
        bool HasEvents(const std::string &binding) const;
        bool Poll(const std::string &binding, Event &eventOut) const;

        static bool Poll(Lock<Read<EventInput>> lock, Entity ent, const std::string &binding, Event &eventOut);
    };

    class EventBindings {
    public:
        EventBindings() {}

        struct Binding {
            EntityRef target;
            std::string destQueue;

            std::optional<Event::EventData> setValue;
            std::optional<double> multiplyValue;

            bool operator==(const Binding &other) const {
                return target == other.target && destQueue == other.destQueue && setValue == other.setValue;
            }
        };

        using BindingList = typename std::vector<Binding>;

        void CopyBindings(const EventBindings &src);

        void Bind(std::string source, const Binding &binding);
        void Bind(std::string source, EntityRef target, std::string dest);
        void Unbind(std::string source, EntityRef target, std::string dest);
        void UnbindSource(std::string source);
        void UnbindTarget(EntityRef target);
        void UnbindDest(EntityRef target, std::string dest);

        const BindingList *Lookup(const std::string source) const;
        std::vector<std::string> GetBindingNames() const;

        static size_t SendEvent(SendEventsLock lock,
            const EntityRef &target,
            const std::string &bindingName,
            const Event &event,
            size_t depth = 0);

        template<typename T>
        static inline size_t SendEvent(SendEventsLock lock, const std::string &name, const Entity &source, T data) {
            return SendEvent(lock, source, name, Event{name, source, data});
        }

    private:
        robin_hood::unordered_map<std::string, BindingList> sourceToDest;
    };

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str, const Name &scope = Name());

    static Component<EventInput> ComponentEventInput("event_input");
    static Component<EventBindings> ComponentEventBindings("event_bindings");

    template<>
    bool Component<EventInput>::Load(const EntityScope &scope, EventInput &dst, const picojson::value &src);
    template<>
    bool Component<EventBindings>::Load(const EntityScope &scope, EventBindings &dst, const picojson::value &src);
    template<>
    void Component<EventInput>::Apply(const EventInput &src, Lock<AddRemove> lock, Entity dst);
    template<>
    void Component<EventBindings>::Apply(const EventBindings &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
