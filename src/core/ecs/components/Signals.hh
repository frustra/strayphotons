#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <limits>
#include <map>
#include <robin_hood.h>
#include <string>

namespace ecs {
    static const size_t MAX_SIGNAL_BINDING_DEPTH = 5;

    using ReadSignalsLock = Lock<Read<Name, SignalOutput, SignalBindings, FocusLayer, FocusLock>>;

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
        enum class CombineOperator { ADD, MIN, MAX, MULTIPLY, COUNT, BINARY_AND, BINARY_OR };

        SignalBindings() {}

        using Binding = typename std::pair<EntityRef, std::string>;
        struct BindingList {
            BindingList(std::initializer_list<Binding> sources = {})
                : operation(CombineOperator::ADD), sources(sources) {}
            BindingList(CombineOperator operation, std::initializer_list<Binding> sources = {})
                : operation(operation), sources(sources) {}

            CombineOperator operation;
            std::vector<Binding> sources;
        };

        void CopyBindings(const SignalBindings &src);

        void SetCombineOperation(const std::string &name, CombineOperator operation);
        void Bind(const std::string &name, EntityRef origin, std::string source);
        void Unbind(const std::string &name, EntityRef origin, std::string source);
        void UnbindAll(const std::string &name);
        void UnbindOrigin(EntityRef origin);
        void UnbindSource(EntityRef origin, std::string source);

        const BindingList *Lookup(const std::string name) const;
        static double GetSignal(ReadSignalsLock lock, Entity ent, const std::string &name, size_t depth = 0);
        std::vector<std::string> GetBindingNames() const;

    private:
        robin_hood::unordered_map<std::string, BindingList> destToSource;
    };

    std::pair<ecs::Name, std::string> ParseSignalString(const std::string &str, const Name &scope = Name());

    std::ostream &operator<<(std::ostream &out, const SignalBindings::CombineOperator &v);

    static Component<SignalOutput> ComponentSignalReceiver("signal_output");
    static Component<SignalBindings> ComponentSignalBindings("signal_bindings");

    template<>
    bool Component<SignalOutput>::Load(const EntityScope &scope, SignalOutput &dst, const picojson::value &src);
    template<>
    bool Component<SignalBindings>::Load(const EntityScope &scope, SignalBindings &dst, const picojson::value &src);
    template<>
    void Component<SignalOutput>::Apply(const SignalOutput &src, Lock<AddRemove> lock, Entity dst);
    template<>
    void Component<SignalBindings>::Apply(const SignalBindings &src, Lock<AddRemove> lock, Entity dst);
} // namespace ecs
