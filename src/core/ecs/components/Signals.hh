#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

#include <limits>
#include <map>
#include <robin_hood.h>
#include <string>

namespace ecs {
    static const size_t MAX_SIGNAL_BINDING_DEPTH = 5;

    class SignalOutput {
    public:
        void SetSignal(const std::string &name, double value);
        void ClearSignal(const std::string &name);
        bool HasSignal(const std::string &name) const;
        double GetSignal(const std::string &name) const;
        const std::map<std::string, double> &GetSignals() const;

    private:
        std::map<std::string, double> signals;
    };

    class SignalBindings {
    public:
        enum class CombineOperator { ADD, MIN, MAX, MULTIPLY, BINARY_AND, BINARY_OR };

        SignalBindings() {}

        using Binding = typename std::pair<NamedEntity, std::string>;
        struct BindingList {
            BindingList(std::initializer_list<Binding> sources = {})
                : operation(CombineOperator::ADD), sources(sources) {}
            BindingList(CombineOperator operation, std::initializer_list<Binding> sources = {})
                : operation(operation), sources(sources) {}

            CombineOperator operation;
            std::vector<Binding> sources;
        };

        void SetCombineOperation(const std::string &name, CombineOperator operation);
        void Bind(const std::string &name, NamedEntity origin, std::string source);
        void Unbind(const std::string &name, NamedEntity origin, std::string source);
        void UnbindAll(const std::string &name);
        void UnbindOrigin(NamedEntity origin);
        void UnbindSource(NamedEntity origin, std::string source);

        const BindingList *Lookup(const std::string name) const;
        static double GetSignal(Lock<Read<Name, SignalOutput, SignalBindings, FocusLayer, FocusLock>> lock,
                                Tecs::Entity ent,
                                const std::string &name,
                                size_t depth = 0);

    private:
        robin_hood::unordered_map<std::string, BindingList> destToSource;
    };

    std::ostream &operator<<(std::ostream &out, const SignalBindings::CombineOperator &v);

    static Component<SignalOutput> ComponentSignalReceiver("signal_output");
    static Component<SignalBindings> ComponentSignalBindings("signal_bindings");

    template<>
    bool Component<SignalOutput>::Load(Lock<Read<ecs::Name>> lock, SignalOutput &dst, const picojson::value &src);
    template<>
    bool Component<SignalBindings>::Load(Lock<Read<ecs::Name>> lock, SignalBindings &dst, const picojson::value &src);
} // namespace ecs
