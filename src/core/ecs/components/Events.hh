#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"
#include "ecs/NamedEntity.hh"

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <queue>
#include <robin_hood.h>
#include <set>
#include <string>
#include <variant>

namespace ecs {
    struct Event {
        using EventData = std::variant<bool, char, int, double, glm::vec2, NamedEntity, std::string>;
        std::string name;
        NamedEntity source;
        EventData data;

        Event() {}
        Event(const std::string &name, const NamedEntity &source) : name(name), source(source), data(true) {}

        template<typename T>
        Event(const std::string &name, const NamedEntity &source, T data) : name(name), source(source), data(data) {}

        friend std::ostream &operator<<(std::ostream &out, const EventData &v);
    };

    static inline std::ostream &operator<<(std::ostream &out, const Event::EventData &v) {
        std::visit(
            [&](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, glm::vec2>) {
                    out << glm::to_string(arg);
                } else if constexpr (std::is_same_v<T, Tecs::Entity>) {
                    out << "Entity(" << arg.id << ")";
                } else if constexpr (std::is_same_v<T, std::string>) {
                    out << "\"" << arg << "\"";
                } else {
                    out << typeid(arg).name() << "(" << arg << ")";
                }
            },
            v);
        return out;
    }

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
        bool Add(const std::string &binding, const Event &event);
        bool HasEvents(const std::string &binding) const;
        bool Poll(const std::string &binding, Event &eventOut);

        static bool Poll(Lock<Write<EventInput>> lock, Tecs::Entity ent, const std::string &binding, Event &eventOut);
    };

    class EventBindings {
    public:
        EventBindings() {}

        using Binding = typename std::pair<NamedEntity, std::string>;
        using BindingList = typename std::vector<Binding>;

        void Bind(std::string source, NamedEntity target, std::string dest);
        void Unbind(std::string source, NamedEntity target, std::string dest);
        void UnbindSource(std::string source);
        void UnbindTarget(NamedEntity target);
        void UnbindDest(NamedEntity target, std::string dest);

        const BindingList *Lookup(const std::string source) const;
        void SendEvent(Lock<Read<Name>, Write<EventInput>> lock, const Event &event) const;
        template<typename T>
        inline void SendEvent(Lock<Read<Name>, Write<EventInput>> lock,
                              const std::string &name,
                              const NamedEntity &source,
                              T data) const {
            SendEvent(lock, Event(name, source, data));
        }

    private:
        robin_hood::unordered_map<std::string, BindingList> sourceToDest;
    };

    static Component<EventInput> ComponentEventInput("event_input");
    static Component<EventBindings> ComponentEventBindings("event_bindings");

    template<>
    bool Component<EventInput>::Load(Lock<Read<ecs::Name>> lock, EventInput &dst, const picojson::value &src);
    template<>
    bool Component<EventBindings>::Load(Lock<Read<ecs::Name>> lock, EventBindings &dst, const picojson::value &src);
} // namespace ecs
