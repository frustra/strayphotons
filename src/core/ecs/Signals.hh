#pragma once

#include <ecs/Ecs.hh>
#include <string>
#include <tuple>

namespace ecs {
    std::tuple<std::string, std::string> ParseSignal(std::string name);
    double FindSignal(Lock<Read<Name, SignalOutput>> lock, std::string name);
} // namespace ecs
