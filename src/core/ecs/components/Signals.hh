#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpression.hh"

#include <limits>
#include <map>
#include <robin_hood.h>
#include <string>

namespace ecs {
    static const size_t MAX_SIGNAL_BINDING_DEPTH = 10;

    struct SignalOutput {
        void SetSignal(const std::string &name, double value);
        void ClearSignal(const std::string &name);
        bool HasSignal(const std::string &name) const;
        double GetSignal(const std::string &name) const;

        robin_hood::unordered_map<std::string, double> signals;
    };

    struct SignalBindings {
        void SetBinding(const std::string &name, const std::string &expr, const Name &scope = Name());
        void SetBinding(const std::string &name, EntityRef entity, const std::string &signalName);
        void ClearBinding(const std::string &name);
        bool HasBinding(const std::string &name) const;
        const SignalExpression &GetBinding(const std::string &name) const;

        static double GetSignal(ReadSignalsLock lock, Entity ent, const std::string &name, size_t depth = 0);

        robin_hood::unordered_map<std::string, SignalExpression> bindings;
    };

    static StructMetadata MetadataSignalOutput(typeid(SignalOutput), StructField::New(&SignalOutput::signals));
    static Component<SignalOutput> ComponentSignalReceiver("signal_output", MetadataSignalOutput);
    template<>
    void Component<SignalOutput>::Apply(const SignalOutput &src, Lock<AddRemove> lock, Entity dst);

    static StructMetadata MetadataSignalBindings(typeid(SignalBindings),
        StructField::New(&SignalBindings::bindings, ~FieldAction::AutoApply));
    static Component<SignalBindings> ComponentSignalBindings("signal_bindings", MetadataSignalBindings);
    template<>
    void Component<SignalBindings>::Apply(const SignalBindings &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
