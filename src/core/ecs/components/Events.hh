#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalExpression.hh"

#include <algorithm>
#include <optional>
#include <robin_hood.h>
#include <string>

namespace ecs {
    static const size_t MAX_EVENT_BINDING_DEPTH = 10;

    using SendEventsLock =
        Lock<Read<Name, FocusLock, EventBindings, EventInput, Signals, SignalBindings, SignalOutput>>;

    struct EventInput {
        EventInput() {}

        void Register(Lock<Write<EventInput>> lock, const EventQueueRef &queue, const std::string &binding);
        void Unregister(const EventQueueRef &queue, const std::string &binding);

        /**
         * Adds an event to any matching event input queues.
         *
         * A transactionId can be provided to synchronize event visibility with transactions.
         * If no transactionId is provided, this event will be immediately visible to all transactions.
         */
        size_t Add(const Event &event, size_t transactionId = 0) const;
        size_t Add(const AsyncEvent &event) const;
        static bool Poll(Lock<Read<EventInput>> lock, const EventQueueRef &queue, Event &eventOut);

        robin_hood::unordered_map<std::string, std::vector<EventQueueRef>> events;
    };

    static StructMetadata MetadataEventInput(typeid(EventInput), "event_input");
    static Component<EventInput> ComponentEventInput(MetadataEventInput);

    struct EventDest {
        EntityRef target;
        std::string queueName;

        bool operator==(const EventDest &) const = default;
    };

    static StructMetadata MetadataEventDest(typeid(EventDest), "EventDest");
    template<>
    bool StructMetadata::Load<EventDest>(EventDest &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventDest>(const EntityScope &scope,
        picojson::value &dst,
        const EventDest &src,
        const EventDest *def);
    template<>
    void StructMetadata::SetScope<EventDest>(EventDest &dst, const EntityScope &scope);

    struct EventBindingActions {
        std::optional<SignalExpression> filterExpr;
        std::vector<SignalExpression> modifyExprs;
        std::optional<EventData> setValue;

        explicit operator bool() const {
            return filterExpr.has_value() || !modifyExprs.empty() || setValue.has_value();
        }

        bool operator==(const EventBindingActions &) const = default;
    };

    static StructMetadata MetadataEventBindingActions(typeid(EventBindingActions),
        "EventBindingActions",
        StructField::New("filter", &EventBindingActions::filterExpr),
        StructField::New("modify", &EventBindingActions::modifyExprs),
        StructField::New("set_value", &EventBindingActions::setValue));

    struct EventBinding {
        std::vector<EventDest> outputs;

        EventBindingActions actions;

        bool operator==(const EventBinding &) const = default;
    };

    static StructMetadata MetadataEventBinding(typeid(EventBinding),
        "EventBinding",
        StructField::New("outputs", &EventBinding::outputs),
        StructField::New(&EventBinding::actions));
    template<>
    bool StructMetadata::Load<EventBinding>(EventBinding &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventBinding>(const EntityScope &scope,
        picojson::value &dst,
        const EventBinding &src,
        const EventBinding *def);
    template<>
    void StructMetadata::DefineSchema<EventBinding>(picojson::value &dst, sp::json::SchemaTypeReferences *references);

    class EventBindings {
    public:
        EventBindings() {}

        EventBinding &Bind(std::string source, const EventBinding &binding);
        EventBinding &Bind(std::string source, EntityRef target, std::string dest);
        void Unbind(std::string source, EntityRef target, std::string dest);

        static size_t SendEvent(const DynamicLock<SendEventsLock> &lock,
            const EntityRef &target,
            const Event &event,
            size_t depth = 0);
        static size_t SendEvent(const DynamicLock<SendEventsLock> &lock,
            const EntityRef &target,
            const AsyncEvent &event,
            size_t depth = 0);

        using BindingList = typename std::vector<EventBinding>;
        robin_hood::unordered_map<std::string, BindingList> sourceToDest;
    };

    static StructMetadata MetadataEventBindings(typeid(EventBindings),
        "event_bindings",
        StructField::New(&EventBindings::sourceToDest, FieldAction::AutoSave));
    static Component<EventBindings> ComponentEventBindings(MetadataEventBindings);
    template<>
    bool StructMetadata::Load<EventBindings>(EventBindings &dst, const picojson::value &src);
    template<>
    void Component<EventBindings>::Apply(EventBindings &dst, const EventBindings &src, bool liveTarget);

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str);
} // namespace ecs
