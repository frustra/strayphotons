/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalRef.hh"

#include <limits>
#include <map>
#include <robin_hood.h>
#include <set>
#include <string>

namespace ecs {
    static const size_t MAX_SIGNAL_BINDING_DEPTH = 10;

    struct Signals {
        struct Signal {
            double value;
            SignalExpression expr;
            SignalRef ref;

            Signal() : value(-std::numeric_limits<double>::infinity()), expr() {}
            Signal(double value, const SignalRef &ref) : value(value), expr() {
                if (!std::isinf(value)) this->ref = ref;
            }
            Signal(const SignalExpression &expr, const SignalRef &ref)
                : value(-std::numeric_limits<double>::infinity()), expr(expr) {
                if (expr) this->ref = ref;
            }
        };

        std::vector<Signal> signals;
        std::multimap<Entity, size_t> entityMapping;
        std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> freeIndexes;

        size_t NewSignal(const Lock<> &lock, const SignalRef &ref, double value);
        size_t NewSignal(const Lock<> &lock, const SignalRef &ref, const SignalExpression &expr);
        void FreeSignal(const Lock<> &lock, size_t index);
        void FreeEntitySignals(const Lock<> &lock, Entity entity);
    };

    struct SignalKey {
        EntityRef entity;
        std::string signalName;

        SignalKey() {}
        SignalKey(const EntityRef &entity, const std::string_view &signalName);
        SignalKey(const std::string_view &str, const EntityScope &scope = Name());

        bool Parse(const std::string_view &str, const EntityScope &scope);

        std::string String() const {
            if (!entity) return signalName;
            return entity.Name().String() + "/" + signalName;
        }

        explicit operator bool() const {
            return entity && !signalName.empty();
        }

        bool operator==(const SignalKey &) const = default;
        bool operator<(const SignalKey &other) const {
            return entity == other.entity ? signalName < other.signalName : entity < other.entity;
        }
    };

    std::ostream &operator<<(std::ostream &out, const SignalKey &v);
} // namespace ecs

TECS_GLOBAL_COMPONENT(ecs::Signals);

namespace std {
    template<>
    struct hash<ecs::SignalKey> {
        std::size_t operator()(const ecs::SignalKey &key) const;
    };
} // namespace std

namespace ecs {
    // SignalOutput is used for staging entities only. See SignalManager for live signals.
    struct SignalOutput {
        robin_hood::unordered_map<std::string, double> signals;
    };

    // SignalBindings is used for staging entities only. See SignalManager for live signals.
    struct SignalBindings {
        robin_hood::unordered_map<std::string, SignalExpression> bindings;
    };

    static Component<SignalOutput> ComponentSignalOutput({typeid(SignalOutput),
        "signal_output",
        R"(
The `signal_output` component stores a list of mutable signal values by name.  
These values are stored as 64-bit double floating-point numbers that exist continuously over time, 
and can be sampled by any [SignalExpression](#signalexpression-type).  
Signal outputs can be written to by scripts and have their value changed in response to events in the world at runtime.

> [!NOTE]
> If a signal is defined in both the `signal_output` component and [`signal_bindings`](#signal_bindings-component) 
> of the same entity, the `signal_output` will take priority and override the binding.

Signal output components can have their initial values defined like this:
```json
"signal_output": {
    "value1": 10.0,
    "value2": 42.0,
    "other": 0.0
}
```
In the above case, setting the "other" output signal to `0.0` will override any `signal_bindings` named "other".
)",
        StructField::New(&SignalOutput::signals, ~FieldAction::AutoApply)});
    template<>
    void Component<SignalOutput>::Apply(SignalOutput &dst, const SignalOutput &src, bool liveTarget);

    static Component<SignalBindings> ComponentSignalBindings({typeid(SignalBindings),
        "signal_bindings",
        R"(
A signal binding is a read-only signal who's value is determined by a [SignalExpression](#signalexpression-type). 
Signal bindings can be referenced the same as signals from the [`signal_output` component](#signal_output-component).

Signal bindings are used to forward state between entities, as well as make calculations about the state of the world.  
Each signal represents a continuous value over time. When read, a binding will have its expression evaluated atomically 
such that data is synchronized between signals and between entities.

A simple signal binding to alias some values from another entity might look like this:
```json
"signal_bindings": {
    "move_forward": "input:keyboard/key_w",
    "move_back": "input:keyboard/key_s"
    "move_left": "input:keyboard/key_a",
    "move_right": "input:keyboard/key_d",
}
```
This will map the *WASD* keys to movement signals on the local entity, decoupling the input source from the game logic.  
You can see more examples of this being used in 
[Stray Photon's Input Bindings](https://github.com/frustra/strayphotons/blob/master/assets/default_input_bindings.json).

> [!WARNING]
> Signal bindings may reference other signal bindings, which will be evaluated recursively up until a maximum depth.  
> Signal expressions should not contain self-referential loops or deep reference trees to avoid unexpected `0.0` evaluations.

> [!NOTE]
> Referencing a missing entity or missing signal will result in a value of `0.0`.  
> If a signal is defined in both the [`signal_output` component](#signal_output-component) and `signal_bindings` component 
> of the same entity, the `signal_output` will take priority and override the binding.

A more complex set of bindings could be added making use of the [SignalExpression](#signalexpression-type) syntax to 
calculate an X/Y movement, and combine it with a joystick input:
```json
"signal_bindings": {
    "move_x": "player/move_right - player/move_left + vr:controller_right/actions_main_in_movement.x",
    "move_y": "player/move_forward - player/move_back + vr:controller_right/actions_main_in_movement.y"
}
```
Depending on the source of the signals, functions like `min(1, x)` and `max(-1, x)` could be added to clamp movement speed.  
Extra multipliers could also be added to adjust joystick senstiivity, movement speed, or flip axes.

For binding state associated with a time, [`event_bindings`](#event_bindings-component) are used instead of signals.
)",
        StructField::New(&SignalBindings::bindings, ~FieldAction::AutoApply)});
    template<>
    void Component<SignalBindings>::Apply(SignalBindings &dst, const SignalBindings &src, bool liveTarget);
} // namespace ecs
