#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalRef.hh"

#include <limits>
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

            Signal() : value(-std::numeric_limits<double>::infinity()) {}
            Signal(double value, const SignalRef &ref) : value(value) {
                if (!std::isinf(value)) this->ref = ref;
            }
            Signal(const SignalExpression &expr, const SignalRef &ref)
                : value(-std::numeric_limits<double>::infinity()), expr(expr) {
                if (expr) this->ref = ref;
            }
        };

        std::vector<Signal> signals;
        std::set<size_t> freeIndexes;

        size_t NewSignal(const SignalRef &ref, double value);
        size_t NewSignal(const SignalRef &ref, const SignalExpression &expr);
        void FreeSignal(size_t index);
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

    static StructMetadata MetadataSignalOutput(typeid(SignalOutput),
        "signal_output",
        "",
        StructField::New(&SignalOutput::signals, ~FieldAction::AutoApply));
    static Component<SignalOutput> ComponentSignalOutput(MetadataSignalOutput);
    template<>
    void Component<SignalOutput>::Apply(SignalOutput &dst, const SignalOutput &src, bool liveTarget);

    static StructMetadata MetadataSignalBindings(typeid(SignalBindings),
        "signal_bindings",
        "",
        StructField::New(&SignalBindings::bindings, ~FieldAction::AutoApply));
    static Component<SignalBindings> ComponentSignalBindings(MetadataSignalBindings);
    template<>
    void Component<SignalBindings>::Apply(SignalBindings &dst, const SignalBindings &src, bool liveTarget);
} // namespace ecs
