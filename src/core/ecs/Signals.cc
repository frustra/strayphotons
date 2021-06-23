#include "Signals.hh"

#include <ecs/EcsImpl.hh>

namespace ecs {
    std::tuple<std::string, std::string> ParseSignal(std::string name) {
        size_t delimiter = name.find_last_of('.');
        std::string entityName = name;
        std::string signalName = "value";
        if (delimiter != std::string::npos) {
            entityName = name.substr(0, delimiter);
            signalName = name.substr(delimiter + 1);
        }
        return std::make_tuple(entityName, signalName);
    }

    double FindSignal(Lock<Read<Name, SignalOutput>> lock, std::string name) {
        auto [entityName, signalName] = ParseSignal(name);
        auto entity = ecs::EntityWith<Name>(lock, entityName);
        if (entity && entity.Has<SignalOutput>(lock)) { return entity.Get<SignalOutput>(lock).GetSignal(signalName); }

        return 0.0;
    }
} // namespace ecs
