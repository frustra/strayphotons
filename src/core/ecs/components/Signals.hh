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

        template<typename LockType>
        static inline double GetSignal(LockType lock, Entity ent, const std::string &name, size_t depth = 0) {
            if (depth > MAX_SIGNAL_BINDING_DEPTH) {
                Errorf("Max signal binding depth exceeded: %s", name);
                return 0.0f;
            }

            if (ent.Has<SignalOutput>(lock)) {
                if constexpr (Tecs::is_entity_lock<LockType>()) {
                    if (ent == lock.entity) {
                        auto &signalOutput = lock.Get<const SignalOutput>();
                        if (signalOutput.HasSignal(name)) return signalOutput.GetSignal(name);
                    } else {
                        auto &signalOutput = ent.Get<const SignalOutput>(lock);
                        if (signalOutput.HasSignal(name)) return signalOutput.GetSignal(name);
                    }
                } else {
                    auto &signalOutput = ent.Get<const SignalOutput>(lock);
                    if (signalOutput.HasSignal(name)) return signalOutput.GetSignal(name);
                }
            }
            if (!ent.Has<SignalBindings>(lock)) return 0.0;

            if constexpr (Tecs::is_entity_lock<LockType>()) {
                if (ent == lock.entity) {
                    auto &bindings = lock.Get<const SignalBindings>();
                    return bindings.GetBinding(name).Evaluate(lock, depth);
                } else {
                    auto &bindings = ent.Get<const SignalBindings>(lock);
                    return bindings.GetBinding(name).Evaluate(lock, depth);
                }
            } else {
                auto &bindings = ent.Get<const SignalBindings>(lock);
                return bindings.GetBinding(name).Evaluate(lock, depth);
            }
        }

        template<typename... Permissions>
        static inline double GetSignal(EntityLock<Permissions...> entLock, const std::string &name, size_t depth = 0) {
            return GetSignal(entLock, entLock.entity, name, depth);
        }

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
