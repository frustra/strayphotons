#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/components/Focus.hh"

#include <optional>
#include <robin_hood.h>
#include <string>

namespace ecs {
    static const size_t MAX_EVENT_BINDING_DEPTH = 10;

    using SendEventsLock = Lock<Read<Name, FocusLock, EventBindings, EventInput>>;

    struct EventInput {
        EventInput() {}

        void Register(Lock<Write<EventInput>> lock, const EventQueueRef &queue, const std::string &binding);
        void Unregister(const EventQueueRef &queue, const std::string &binding);

        size_t Add(const Event &event) const;
        static bool Poll(Lock<Read<EventInput>> lock, const EventQueueRef &queue, Event &eventOut);

        robin_hood::unordered_map<std::string, std::vector<std::shared_ptr<EventQueue>>> events;
    };

    struct EventBinding {
        EntityRef target;
        std::string destQueue;

        FocusLayer ifFocused = FocusLayer::Always;
        std::optional<Event::EventData> setValue;
        std::optional<double> multiplyValue;

        bool operator==(const EventBinding &) const = default;
    };

    static StructMetadata MetadataEventBinding(typeid(EventBinding),
        StructField::New("if_focused", &EventBinding::ifFocused),
        StructField::New("multiply_value", &EventBinding::multiplyValue));
    template<>
    bool StructMetadata::Load<EventBinding>(const EntityScope &scope, EventBinding &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventBinding>(const EntityScope &scope, picojson::value &dst, const EventBinding &src);

    class EventBindings {
    public:
        EventBindings() {}

        void Bind(std::string source, const EventBinding &binding);
        void Bind(std::string source, EntityRef target, std::string dest);
        void Unbind(std::string source, EntityRef target, std::string dest);
        void UnbindSource(std::string source);
        void UnbindTarget(EntityRef target);
        void UnbindDest(EntityRef target, std::string dest);

        static size_t SendEvent(SendEventsLock lock, const EntityRef &target, const Event &event, size_t depth = 0);

        using BindingList = typename std::vector<EventBinding>;
        robin_hood::unordered_map<std::string, BindingList> sourceToDest;
    };

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str, const Name &scope = Name());

    static StructMetadata MetadataEventInput(typeid(EventInput));
    static Component<EventInput> ComponentEventInput("event_input", MetadataEventInput);

    static StructMetadata MetadataEventBindings(typeid(EventBindings),
        StructField::New(&EventBindings::sourceToDest, ~FieldAction::AutoApply));
    static Component<EventBindings> ComponentEventBindings("event_bindings", MetadataEventBindings);

    template<>
    void Component<EventBindings>::Apply(EventBindings &dst, const EventBindings &src, bool liveTarget);
} // namespace ecs
