#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

#include <limits>
#include <map>
#include <robin_hood.h>
#include <string>

namespace ecs {
    class SignalOutput {
    public:
        void SetSignal(const std::string &name, double value);
        void ClearSignal(const std::string &name);
        double GetSignal(const std::string &name) const;
        const std::map<std::string, double> &GetSignals() const;

    private:
        std::map<std::string, double> signals;
    };

    class SignalBindings {
    public:
        enum class CombineOperator { ADD, MIN, MAX, MULTIPLY, BINARY_AND, BINARY_OR };

        SignalBindings(CombineOperator operation = CombineOperator::ADD) : operation(operation) {}

        using Binding = typename std::pair<NamedEntity, std::string>;
        using BindingList = typename std::vector<Binding>;

        void Bind(std::string name, NamedEntity origin, std::string source);
        void Unbind(std::string name, NamedEntity origin, std::string source);
        void UnbindAll(std::string name);
        void UnbindOrigin(NamedEntity origin);
        void UnbindSource(NamedEntity origin, std::string source);

        const BindingList *Lookup(const std::string name) const;
        double GetSignal(Lock<Read<Name, SignalOutput>> lock, const std::string &name) const;

        CombineOperator operation;

    private:
        robin_hood::unordered_map<std::string, BindingList> destToSource;
    };

    static Component<SignalOutput> ComponentSignalReceiver("signal_output");
    static Component<SignalBindings> ComponentSignalBindings("signal_bindings");

    template<>
    bool Component<SignalOutput>::Load(Lock<Read<ecs::Name>> lock, SignalOutput &dst, const picojson::value &src);
    template<>
    bool Component<SignalBindings>::Load(Lock<Read<ecs::Name>> lock, SignalBindings &dst, const picojson::value &src);
} // namespace ecs
