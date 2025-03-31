/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/LockFreeMutex.hh"
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

    using SendEventsLock = Lock<
        Read<Name, FocusLock, EventBindings, EventInput, Signals, SignalBindings, SignalOutput>>;

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

        robin_hood::unordered_map<std::string, std::vector<EventQueueWeakRef>> events;
    };

    static Component<EventInput> ComponentEventInput({typeid(EventInput), "event_input", R"(
For an entity to receive events (not just forward them), it must have an `event_input` component.  
This component stores a list of event queues that have been registered to receive events on a per-entity basis.

No configuration is stored for the `event_input` component. Scripts and other systems programmatically register 
their own event queues as needed.
)"});

    struct EventDest {
        EntityRef target;
        std::string queueName;

        bool operator==(const EventDest &) const = default;
    };

    static StructMetadata MetadataEventDest(typeid(EventDest),
        "EventDest",
        "An event destination in the form of a string: `\"target_entity/event/input\"`");
    template<>
    bool StructMetadata::Load<EventDest>(EventDest &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<EventDest>(const EntityScope &scope,
        picojson::value &dst,
        const EventDest &src,
        const EventDest *def);
    template<>
    void StructMetadata::SetScope<EventDest>(EventDest &dst, const EntityScope &scope);
    template<>
    void StructMetadata::DefineSchema<EventDest>(picojson::value &dst, sp::json::SchemaTypeReferences *references);

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
        "",
        StructField::New("filter",
            "If a `filter` expression is specified, only events for which the filter evaluates to true (>= 0.5) are "
            "forwarded to the output. Event data can be accessed in the filter expression via the `event` keyword "
            "(e.g. `event.y > 2`)",
            &EventBindingActions::filterExpr),
        StructField::New("set_value",
            "If `set_value` is specified, the event's data and type will be changed to the provided value. "
            "`set_value` occurs before the `modify` expression is evaluated.",
            &EventBindingActions::setValue),
        StructField::New("modify",
            "One or more `modify` expressions can be specified to manipulate the contents of events as they flow "
            "through an binding. 1 expression is evaluated per 'element' of the input event type (i.e. 3 exprs for "
            "vec3, 1 expr for bool, etc...). Input event data is provided via the `event` expression keyword, and the "
            "result of each expression is then converted back to the original event type before being used as the "
            "new event data.  "
            "As an example, the following `modify` expression will scale an input vec3 event by 2 on all axes: "
            "`[\"event.x * 2\", \"event.y * 2\", \"event.z * 2\"]`.",
            &EventBindingActions::modifyExprs));

    struct EventBinding {
        std::vector<EventDest> outputs;

        EventBindingActions actions;

        bool operator==(const EventBinding &) const = default;
    };

    static StructMetadata MetadataEventBinding(typeid(EventBinding),
        "EventBinding",
        "",
        StructField::New("outputs",
            "One or more event queue targets to send events to. "
            "If only a single output is needed, it can be specified directly as: "
            "`\"outputs\": \"target_entity/target/event/queue\"`",
            &EventBinding::outputs),
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

    static Component<EventBindings> ComponentEventBindings({typeid(EventBindings),
        "event_bindings",
        R"(
Event bindings, along with [`signal_bindings`](#signal_bindings-component), are the main ways entites 
communicate with eachother. When an event is generated at an entity, it will first be delivered to 
any local event queues in the [`event_input` Component](#event_input-component). 
Then it will be forwarded through any matching event bindings to be delivered to other entities.

In their most basic form, event bindings will simply forward events to a new target:
```json
"event_bindings": {
    "/button1/action/press": "player:player/action/jump",
    "/trigger/object/enter": ["scene:object1/notify/react", "scene:object2/notify/react"]
}
```
For the above bindings, when the input event `/button1/action/press` is generated at the local entity, 
it will be forwarded to the `player:player` entity as the `/action/jump` event with the same event data. 
Similarly, the `/trigger/object/enter` will be forwarded to both the `scene:object1` and `scene:object2` 
entities as the `/notify/react` event, again with the original event data intact.

> [!WARNING]
> Events are forwarded recursively until a maximum binding depth is reached.  
> Bindings should not contain loops or deep binding trees, otherwise events will be dropped.

More advanced event bindings can also filter and modify event data as is passes through them. 
For example, the following binding is evaluated for each `/keyboard/key/v` input event:
```json
"event_bindings": {
    "/keyboard/key/v": {
        "outputs": "console:input/action/run_command",
        "filter": "is_focused(Game) && event",
        "set_value": "togglesignal player:player/move_noclip"
    }
}
```
Key input events from the `input:keyboard` entity have a bool data type, and are generated for both the key down 
and key up events (signified by a **true** and **false** value respectively).

The above binding specifies a filter expression that will only forward events while the `Game` focus layer is active,
and will only forward key down events (true data values).

The event data is then replaced with the string `"togglesignal player:player/move_noclip"`, and forwarded to the `console:input` entity
as the `/action/run_command` event. Upon receiving this event, the `console:input` entity will execute the provided string in the console.
)",
        StructField::New(&EventBindings::sourceToDest, FieldAction::AutoSave)});
    template<>
    bool StructMetadata::Load<EventBindings>(EventBindings &dst, const picojson::value &src);
    template<>
    void Component<EventBindings>::Apply(EventBindings &dst, const EventBindings &src, bool liveTarget);

    std::pair<ecs::Name, std::string> ParseEventString(const std::string &str);
} // namespace ecs
