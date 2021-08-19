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
                    bindings.Bind(source.first, NamedEntity(originName), bind.first);
                }
            }
        }
        return true;
    }

    void SignalOutput::SetSignal(const std::string &name, double value) {
        // Debugf("Setting signal \"%s\" to: %f", name, value);
        signals[name] = value;
    }

    void SignalOutput::ClearSignal(const std::string &name) {
        // Debugf("Clearing signal \"%s\"", name);
        signals.erase(name);
    }

    double SignalOutput::GetSignal(const std::string &name) const {
        auto signal = signals.find(name);
        if (signal != signals.end()) return signal->second;
        return 0.0;
    }

    const std::map<std::string, double> &SignalOutput::GetSignals() const {
        return signals;
    }

    void SignalBindings::Bind(std::string name, NamedEntity origin, std::string source) {
        auto list = destToSource.find(name);
        if (list != destToSource.end()) {
            list->second.emplace_back(origin, source);
        } else {
            destToSource.emplace(name, std::initializer_list<Binding>{Binding(origin, source)});
        }
    }

    void SignalBindings::Unbind(std::string name, NamedEntity origin, std::string source) {
        auto list = destToSource.find(name);
        if (list != destToSource.end()) {
            auto binding = list->second.begin();
            while (binding != list->second.end()) {
                if (binding->first == origin && binding->second == source) {
                    binding = list->second.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    void SignalBindings::UnbindAll(std::string name) {
        destToSource.erase(name);
    }

    void SignalBindings::UnbindOrigin(NamedEntity origin) {
        for (auto &list : destToSource) {
            auto binding = list.second.begin();
            while (binding != list.second.end()) {
                if (binding->first == origin) {
                    binding = list.second.erase(binding);
                } else {
                    binding++;
                }
            }
        }
    }

    void SignalBindings::UnbindSource(NamedEntity origin, std::string source) {
        for (auto &list : destToSource) {
            auto binding = list.second.begin();
            while (binding != list.second.end()) {
                if (binding->first == origin && binding->second == source) {
                    binding = list.second.erase(binding);
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

    double SignalBindings::GetSignal(Lock<Read<Name, SignalOutput>> lock, const std::string &name) const {
        auto bindingList = Lookup(name);
        if (bindingList == nullptr) return 0.0;

        switch (operation) {
        case CombineOperator::ADD:
        case CombineOperator::MIN:
        case CombineOperator::MAX:
        case CombineOperator::MULTIPLY: {
            std::optional<double> output;
            for (auto &signal : *bindingList) {
                auto origin = signal.first.Get(lock);
                if (origin.Has<SignalOutput>(lock)) {
                    auto &signalOutput = origin.Get<SignalOutput>(lock);
                    auto val = signalOutput.GetSignal(signal.second);
                    switch (operation) {
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
            }
            return output.value_or(0.0);
        } break;
        case CombineOperator::BINARY_AND:
        case CombineOperator::BINARY_OR: {
            std::optional<bool> output;
            for (auto &signal : *bindingList) {
                auto origin = signal.first.Get(lock);
                if (origin.Has<SignalOutput>(lock)) {
                    auto &signalOutput = origin.Get<SignalOutput>(lock);
                    bool val = signalOutput.GetSignal(signal.second) >= 0.5;
                    switch (operation) {
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
            }
            return output.value_or(false) ? 1.0 : 0.0;
        } break;
        default:
            sp::Assert(false, "Bad signal combine operator");
            return 0.0;
        }
    }
} // namespace ecs
