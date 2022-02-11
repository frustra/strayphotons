#pragma once

#include "core/Logging.hh"

namespace sp {
    enum class SceneType {
        Async = 0,
        World,
        Player,
        System,
        Count,
    };

    namespace logging {
        template<>
        struct stringify<SceneType> : std::true_type {
            static const char *to_string(const SceneType &t) {
                switch (t) {
                case SceneType::Async:
                    return "Async";
                case SceneType::World:
                    return "World";
                case SceneType::Player:
                    return "Player";
                case SceneType::System:
                    return "System";
                default:
                    return "SceneType::INVALID";
                }
            }
        };
    } // namespace logging
} // namespace sp
