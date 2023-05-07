#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalRef.hh"

#include <limits>
#include <set>
#include <robin_hood.h>
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
    struct SignalOutput {
        // void SetSignal(const SignalRef &name, double value);
        // void ClearSignal(const SignalRef &name);
        // bool HasSignal(const SignalRef &name) const;
        // double GetSignal(const SignalRef &name) const;

        robin_hood::unordered_map<std::string, double> signals;
    };

    struct SignalBindings {
        // void SetBinding(const SignalRef &name, const std::string &expr, const Name &scope = Name());
        // void SetBinding(const SignalRef &name, const SignalRef &signal);
        // void ClearBinding(const SignalRef &name);
        // bool HasBinding(const SignalRef &name) const;
        // const SignalExpression &GetBinding(const SignalRef &name) const;

        // static double GetSignal(const DynamicLock<ReadSignalsLock> &lock, const SignalRef &name, size_t depth = 0);

        robin_hood::unordered_map<std::string, SignalExpression> bindings;
    };

    static StructMetadata MetadataSignalOutput(typeid(SignalOutput), StructField::New(&SignalOutput::signals));
    static Component<SignalOutput> ComponentSignalReceiver("signal_output", MetadataSignalOutput);
    template<>
    void Component<SignalOutput>::Apply(SignalOutput &dst, const SignalOutput &src, bool liveTarget);

    static StructMetadata MetadataSignalBindings(typeid(SignalBindings),
        StructField::New(&SignalBindings::bindings, ~FieldAction::AutoApply));
    static Component<SignalBindings> ComponentSignalBindings("signal_bindings", MetadataSignalBindings);
    template<>
    void Component<SignalBindings>::Apply(SignalBindings &dst, const SignalBindings &src, bool liveTarget);
} // namespace ecs
