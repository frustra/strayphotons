#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Focus.hh"

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
    struct Event {
        using EventData =
            std::variant<bool, char, int, double, glm::vec2, glm::vec3, EntityRef, Tecs::Entity, std::string>;

        std::string name;
        EntityRef source;
        EventData data;

        Event() {}
        Event(const std::string &name, const EntityRef &source) : name(name), source(source), data(true) {}

        template<typename T>
        Event(const std::string &name, const EntityRef &source, T data) : name(name), source(source), data(data) {}

        std::string toString() const;
    };

    std::ostream &operator<<(std::ostream &out, const Event::EventData &v);

    struct EventInput {
        robin_hood::unordered_map<std::string, std::queue<Event>> events;

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
        bool Add(const std::string &binding, const Event &event);
        bool HasEvents(const std::string &binding) const;
        bool Poll(const std::string &binding, Event &eventOut);

        static bool Poll(Lock<Write<EventInput>> lock, Entity ent, const std::string &binding, Event &eventOut);
    };

    class EventBindings {
    public:
        EventBindings() {}

        struct Binding {
            EntityRef target;
            std::string destQueue;

            std::optional<Event::EventData> setValue;

            bool operator==(const Binding &other) const {
                return target == other.target && destQueue == other.destQueue;
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
        void SendEvent(Lock<Read<Name, FocusLayer, FocusLock>, Write<EventInput>> lock, const Event &event) const;
        template<typename T>
        inline void SendEvent(Lock<Read<Name, FocusLayer, FocusLock>, Write<EventInput>> lock,
            const std::string &name,
            const EntityRef &source,
            T data) const {
            SendEvent(lock, Event(name, source, data));
        }
        std::vector<std::string> GetBindingNames() const;

    private:
        robin_hood::unordered_map<std::string, BindingList> sourceToDest;
    };

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str, const Name &scope = Name());

    static Component<EventInput> ComponentEventInput("event_input");
    static Component<EventBindings> ComponentEventBindings("event_bindings");

    template<>
    bool Component<EventInput>::Load(ScenePtr scenePtr, const Name &scope, EventInput &dst, const picojson::value &src);
    template<>
    bool Component<EventBindings>::Load(ScenePtr scenePtr,
        const Name &scope,
        EventBindings &dst,
        const picojson::value &src);
    template<>
    void Component<EventInput>::Apply(const EventInput &src, Lock<AddRemove> lock, Entity dst);
    template<>
    void Component<EventBindings>::Apply(const EventBindings &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
