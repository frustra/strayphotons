#include "Signals.hh"

#include "assets/AssetHelpers.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <optional>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SignalOutput>::Load(Lock<Read<ecs::Name>> lock, SignalOutput &output, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.second.is<bool>()) {
                output.SetSignal(param.first, param.second.get<bool>() ? 1.0 : 0.0);
            } else {
                output.SetSignal(param.first, param.second.get<double>());
            }
        }
        return true;
    }

    template<>
    bool Component<SignalBindings>::Load(Lock<Read<ecs::Name>> lock,
                                         SignalBindings &bindings,
                                         const picojson::value &src) {
        for (auto bind : src.get<picojson::object>()) {
            Tecs::Entity target;
            std::string targetEvent;
            for (auto source : bind.second.get<picojson::object>()) {
                picojson::array originList;
                if (source.second.is<std::string>()) {
                    originList.emplace_back(source.second);
                } else if (source.second.is<picojson::array>()) {
                    originList = source.second.get<picojson::array>();
                } else {
                    sp::Assert(false, "Invalid signal source");
                }
                for (auto origin : originList) {
                    auto originName = origin.get<std::string>();
                    bindings.Bind(bind.first, NamedEntity(originName), source.first);
                }
            }
        }
        return true;
    }

    std::ostream &operator<<(std::ostream &out, const SignalBindings::CombineOperator &v) {
        switch (v) {
        case SignalBindings::CombineOperator::ADD:
            return out << "CombineOperator::ADD";
        case SignalBindings::CombineOperator::MIN:
            return out << "CombineOperator::MIN";
        case SignalBindings::CombineOperator::MAX:
            return out << "CombineOperator::MAX";
        case SignalBindings::CombineOperator::MULTIPLY:
            return out << "CombineOperator::MULTIPLY";
        case SignalBindings::CombineOperator::BINARY_AND:
            return out << "CombineOperator::BINARY_AND";
        case SignalBindings::CombineOperator::BINARY_OR:
            return out << "CombineOperator::BINARY_OR";
        default:
            return out << "CombineOperator::INVALID";
        }
    }

    void SignalOutput::SetSignal(const std::string &name, double value) {
        signals[name] = value;
    }

    void SignalOutput::ClearSignal(const std::string &name) {
        signals.erase(name);
    }

    bool SignalOutput::HasSignal(const std::string &name) const {
        return signals.count(name) > 0;
    }

    double SignalOutput::GetSignal(const std::string &name) const {
        auto signal = signals.find(name);
        if (signal != signals.end()) return signal->second;
        return 0.0;
    }

    const std::map<std::string, double> &SignalOutput::GetSignals() const {
        return signals;
    }

    void SignalBindings::SetCombineOperation(const std::string &name, CombineOperator operation) {
        auto list = destToSource.find(name);
        if (list != destToSource.end()) {
            list->second.operation = operation;
        } else {
            destToSource.emplace(name, BindingList{operation});
        }
    }

    void SignalBindings::Bind(const std::string &name, NamedEntity origin, std::string source) {
        Debugf("Binding %s to %s on %s", name, source, origin.Name());
        auto list = destToSource.find(name);
        if (list != destToSource.end()) {
            list->second.sources.emplace_back(origin, source);
        } else {
            destToSource.emplace(name, BindingList{{Binding(origin, source)}});
        }
    }

    void SignalBindings::Unbind(const std::string &name, NamedEntity origin, std::string source) {
        auto list = destToSource.find(name);
        if (list != destToSource.end()) {
            auto &sources = list->second.sources;
            auto binding = sources.begin();
            while (binding != sources.end()) {
                if (binding->first == origin && binding->second == source) {
                    binding = sources.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    void SignalBindings::UnbindAll(const std::string &name) {
        destToSource.erase(name);
    }

    void SignalBindings::UnbindOrigin(NamedEntity origin) {
        for (auto &list : destToSource) {
            auto &sources = list.second.sources;
            auto binding = sources.begin();
            while (binding != sources.end()) {
                if (binding->first == origin) {
                    binding = sources.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    void SignalBindings::UnbindSource(NamedEntity origin, std::string source) {
        for (auto &list : destToSource) {
            auto &sources = list.second.sources;
            auto binding = sources.begin();
            while (binding != sources.end()) {
                if (binding->first == origin && binding->second == source) {
                    binding = sources.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    const SignalBindings::BindingList *SignalBindings::Lookup(const std::string name) const {
        auto list = destToSource.find(name);
        if (list != destToSource.end()) { return &list->second; }
        return nullptr;
    }

    double SignalBindings::GetSignal(Lock<Read<Name, SignalOutput, SignalBindings, FocusLayer, FocusLock>> lock,
                                     Tecs::Entity ent,
                                     const std::string &name,
                                     size_t depth) {
        if (depth > MAX_SIGNAL_BINDING_DEPTH) {
            Errorf("Max signal binding depth exceeded: %s", name);
            return 0.0f;
        }

        if (depth == 0) {
            const FocusLock *focusLock = nullptr;
            if (lock.Has<FocusLock>()) focusLock = &lock.Get<FocusLock>();
            if (focusLock && ent.Has<FocusLayer>(lock)) {
                auto &layer = ent.Get<FocusLayer>(lock);
                if (!focusLock->HasPrimaryFocus(layer)) return 0.0;
            }
        }

        if (ent.Has<SignalOutput>(lock)) {
            auto &signalOutput = ent.Get<SignalOutput>(lock);
            if (signalOutput.HasSignal(name)) return signalOutput.GetSignal(name);
        }
        if (!ent.Has<SignalBindings>(lock)) return 0.0;

        auto &bindings = ent.Get<SignalBindings>(lock);
        auto bindingList = bindings.Lookup(name);
        if (bindingList == nullptr) return 0.0;

        switch (bindingList->operation) {
        case CombineOperator::ADD:
        case CombineOperator::MIN:
        case CombineOperator::MAX:
        case CombineOperator::MULTIPLY: {
            std::optional<double> output;
            for (auto &signal : bindingList->sources) {
                auto origin = signal.first.Get(lock);
                auto val = SignalBindings::GetSignal(lock, origin, signal.second, depth + 1);
                switch (bindingList->operation) {
                case CombineOperator::ADD:
                    output = output.value_or(0.0) + val;
                    break;
                case CombineOperator::MIN:
                    output = std::min(output.value_or(val), val);
                    break;
                case CombineOperator::MAX:
                    output = std::max(output.value_or(val), val);
                    break;
                case CombineOperator::MULTIPLY:
                    output = output.value_or(1.0) * val;
                    break;
                default:
                    sp::Assert(false, "Bad signal combine operator");
                }
            }
            return output.value_or(0.0);
        } break;
        case CombineOperator::BINARY_AND:
        case CombineOperator::BINARY_OR: {
            std::optional<bool> output;
            for (auto &signal : bindingList->sources) {
                auto origin = signal.first.Get(lock);
                bool val = SignalBindings::GetSignal(lock, origin, signal.second, depth + 1) >= 0.5;
                switch (bindingList->operation) {
                case CombineOperator::BINARY_AND:
                    output = output.value_or(true) && val;
                    break;
                case CombineOperator::BINARY_OR:
                    output = output.value_or(false) || val;
                    break;
                default:
                    sp::Assert(false, "Bad signal combine operator");
                }
            }
            return output.value_or(false) ? 1.0 : 0.0;
        } break;
        default:
            sp::Assert(false, "Bad signal combine operator");
            return 0.0;
        }
    }
} // namespace ecs
