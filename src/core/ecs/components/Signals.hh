#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/StringHandle.hh"

#include <ankerl/unordered_dense.h>
#include <limits>
#include <map>
#include <robin_hood.h>
#include <string>

namespace ecs {
    static const size_t MAX_SIGNAL_BINDING_DEPTH = 10;

    struct SignalOutput {
        void SetSignal(const StringHandle &name, double value);
        void ClearSignal(const StringHandle &name);
        bool HasSignal(const StringHandle &name) const;
        double GetSignal(const StringHandle &name) const;

        // robin_hood::unordered_map<std::string, double> signals;
        robin_hood::unordered_map<StringHandle, double> signals;
    };

    struct SignalBindings {
        void SetBinding(const StringHandle &name, const std::string &expr, const Name &scope = Name());
        void SetBinding(const StringHandle &name, EntityRef entity, const StringHandle &signalName);
        void ClearBinding(const StringHandle &name);
        bool HasBinding(const StringHandle &name) const;
        const SignalExpression &GetBinding(const StringHandle &name) const;

        static double GetSignal(const DynamicLock<ReadSignalsLock> &lock,
            const Entity &ent,
            const StringHandle &name,
            size_t depth = 0);

        // robin_hood::unordered_map<std::string, SignalExpression> bindings;
        robin_hood::unordered_map<StringHandle, SignalExpression> bindings;
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
