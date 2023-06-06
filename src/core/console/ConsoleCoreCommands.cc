/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "console/Console.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"
#include "ecs/EcsImpl.hh"

#include <fstream>
#include <shared_mutex>
#include <sstream>

template<typename Callback>
void mutateEntityTransform(const ecs::EntityRef &entityRef, Callback callback) {
    auto lock = ecs::StartTransaction<ecs::Write<ecs::TransformTree, ecs::TransformSnapshot>>();
    auto entity = entityRef.Get(lock);
    if (!entity.Exists(lock)) {
        Errorf("Entity does not exist: %s", entityRef.Name().String());
    } else if (!entity.Has<ecs::TransformTree, ecs::TransformSnapshot>(lock)) {
        Errorf("Entity has no TransformTree or TransformSnapshot: %s", entityRef.Name().String());
    } else {
        auto &transform = entity.Get<ecs::TransformTree>(lock);
        callback(lock, transform);
        entity.Set<ecs::TransformSnapshot>(lock, transform.GetGlobalTransform(lock));
    }
}

void sp::ConsoleManager::RegisterCoreCommands() {
    funcs.Register("list", "Lists all CVar names, values, and descriptions", [this]() {
        std::shared_lock lock(cvarReadLock);

        for (auto &kv : cvars) {
            auto cvar = kv.second;
            if (cvar->IsValueType()) {
                logging::ConsoleWrite(logging::Level::Log, " > %s = %s", cvar->GetName(), cvar->StringValue());
            } else {
                logging::ConsoleWrite(logging::Level::Log, " > %s (func)", cvar->GetName());
            }

            auto desc = cvar->GetDescription();
            if (desc.size() > 0) {
                logging::ConsoleWrite(logging::Level::Log, " >   %s", desc);
            }
        }
    });

    funcs.Register<uint64, string>("wait", "Queue command for later (wait <ms> <command>)", [](uint64 dt, string cmd) {
        GetConsoleManager().QueueParseAndExecute(cmd, chrono_clock::now() + std::chrono::milliseconds(dt));
    });

    funcs.Register<string, string>("toggle",
        "Toggle a CVar between values (toggle <cvar_name> [<value_a> <value_b>])",
        [this](string cvarName, string args) {
            std::shared_lock lock(cvarReadLock);

            auto cvarit = cvars.find(to_lower_copy(cvarName));
            if (cvarit != cvars.end()) {
                auto cvar = cvarit->second;
                if (cvar->IsValueType()) {
                    std::stringstream stream(args);
                    vector<string> values;
                    string value;
                    while (stream >> value) {
                        values.push_back(value);
                    }
                    cvar->ToggleValue(values.data(), values.size());
                } else {
                    logging::ConsoleWrite(logging::Level::Log, " > '%s' is not a cvar", cvarName);
                }
            } else {
                logging::ConsoleWrite(logging::Level::Log, " > '%s' undefined", cvarName);
            }
        });

    funcs.Register("printfocus", "Print the current focus lock state", []() {
        auto lock = ecs::StartTransaction<ecs::Read<ecs::FocusLock>>();

        if (lock.Has<ecs::FocusLock>()) {
            Logf("Active focus layers: %s", lock.Get<ecs::FocusLock>().String());
        } else {
            Errorf("World does not have a FocusLock");
        }
    });

    funcs.Register<ecs::FocusLayer>("acquirefocus", "Acquire focus for the specified layer", [](ecs::FocusLayer layer) {
        if (layer != ecs::FocusLayer::Never && layer != ecs::FocusLayer::Always) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();

            if (lock.Has<ecs::FocusLock>()) {
                if (!lock.Get<ecs::FocusLock>().AcquireFocus(layer)) {
                    Logf("Failed to acquire focus layer: %s", layer);
                }
            } else {
                Errorf("World does not have a FocusLock");
            }
        }
    });

    funcs.Register<ecs::FocusLayer>("releasefocus", "Release focus for the specified layer", [](ecs::FocusLayer layer) {
        if (layer != ecs::FocusLayer::Never && layer != ecs::FocusLayer::Always) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();

            if (lock.Has<ecs::FocusLock>()) {
                lock.Get<ecs::FocusLock>().ReleaseFocus(layer);
            } else {
                Errorf("World does not have a FocusLock");
            }
        }
    });

    funcs.Register<ecs::EntityRef, glm::vec3>("translate",
        "Moves an entity a relative amount (translate <entity> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, glm::vec3 value) {
            mutateEntityTransform(entityRef, [&](auto lock, ecs::TransformTree &transform) {
                transform.pose.Translate(value);
            });
        });

    funcs.Register<ecs::EntityRef, float, glm::vec3>("rotate",
        "Rotates an entity a relative amount (rotate <entity> <degrees> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, float degrees, glm::vec3 plane) {
            mutateEntityTransform(entityRef, [&](auto lock, ecs::TransformTree &transform) {
                transform.pose.Rotate(glm::radians(degrees), plane);
            });
        });

    funcs.Register<ecs::EntityRef, glm::vec3>("scale",
        "Scales an entity a relative amount (scale <entity> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, glm::vec3 value) {
            mutateEntityTransform(entityRef, [&](auto lock, ecs::TransformTree &transform) {
                transform.pose.Scale(value);
            });
        });

    funcs.Register<string, double>("setsignal",
        "Set a signal value (setsignal <entity>/<signal> <value>)",
        [](string signalStr, double value) {
            ecs::SignalRef signal(signalStr);
            if (!signal) {
                Logf("Invalid signal string: %s", signalStr);
                return;
            }

            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::Signals>>();
            auto &entityRef = signal.GetEntity();
            ecs::Entity entity = entityRef.Get(lock);
            if (!entity) {
                Logf("Signal entity %s not found", entityRef.Name().String());
                return;
            }

            signal.SetValue(lock, value);
        });

    funcs.Register<string, string>("togglesignal",
        "Toggle a signal between values (togglesignal <entity>/<signal> [<value_a> <value_b>])",
        [](string signalStr, string args) {
            ecs::SignalRef signal(signalStr);
            if (!signal) {
                Logf("Invalid signal string: %s", signalStr);
                return;
            }

            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Write<ecs::Signals>>();
            auto &entityRef = signal.GetEntity();
            ecs::Entity entity = entityRef.Get(lock);
            if (!entity) {
                Logf("Signal entity %s not found", entityRef.Name().String());
                return;
            }

            std::stringstream stream(args);
            vector<string> values;
            string value;
            while (stream >> value) {
                values.push_back(value);
            }

            auto signalValue = signal.GetSignal(lock);
            ToggleBetweenValues(signalValue, values.data(), values.size());
            signal.SetValue(lock, signalValue);
        });

    funcs.Register<string>("clearsignal", "Clear a signal value (clearsignal <entity>/<signal>)", [](string signalStr) {
        ecs::SignalRef signal(signalStr);
        if (!signal) {
            Logf("Invalid signal string: %s", signalStr);
            return;
        }

        auto lock = ecs::StartTransaction<ecs::Write<ecs::Signals>>();
        auto &entityRef = signal.GetEntity();
        ecs::Entity entity = entityRef.Get(lock);
        if (!entity) {
            Logf("Signal entity %s not found", entityRef.Name().String());
            return;
        }

        signal.ClearValue(lock);
    });

    funcs.Register<string, string>("sendevent",
        "Send an entity an event (sendevent <entity>/<event> <value>)",
        [](string eventStr, string value) {
            auto [entityName, eventName] = ecs::ParseEventString(eventStr);

            ecs::Event event(eventName, ecs::Entity(), true);
            if (!value.empty()) {
                if (sp::iequals(value, "true")) {
                    event.data = true;
                } else if (sp::iequals(value, "false")) {
                    event.data = false;
                } else if (is_float(value)) {
                    event.data = std::stof(value);
                } else {
                    // Try splitting the value by whitespace to convert to a glm::vecN
                    std::stringstream stream(value);
                    std::array<string, 5> values;
                    int count = 0;
                    for (auto &field : values) {
                        if (!(stream >> field)) break;
                        if (is_float(field)) {
                            count++;
                        } else {
                            // If any field is not a float, treat the value as a string
                            count = -1;
                            break;
                        }
                    }

                    if (count == 2) {
                        event.data = glm::vec2(std::stof(values[0]), std::stof(values[1]));
                    } else if (count == 3) {
                        event.data = glm::vec3(std::stof(values[0]), std::stof(values[1]), std::stof(values[2]));
                    } else if (count == 4) {
                        event.data = glm::vec4(std::stof(values[0]),
                            std::stof(values[1]),
                            std::stof(values[2]),
                            std::stof(values[3]));
                    } else {
                        event.data = value;
                    }
                }
            }

            auto lock = ecs::StartTransaction<ecs::SendEventsLock>();
            auto sent = ecs::EventBindings::SendEvent(lock, entityName, event);
            if (sent == 0) {
                Warnf("No event target found: %s%s", entityName.String(), eventName);
            } else {
                Logf("Sent %u events to %s%s", sent, entityName.String(), eventName);
            }
        });

    funcs.Register<size_t>("tracetecs", "Save an ECS performance trace (tracetecs <time_ms>)", [](size_t timeMs) {
        if (timeMs == 0) {
            Logf("Trace time must be specified in milliseconds.");
            return;
        }

        static std::atomic_bool tracingStarted(false);
        bool current = tracingStarted;
        if (current || !tracingStarted.compare_exchange_strong(current, true)) {
            Logf("A performance trace is already in progress");
            return;
        }

        std::thread([timeMs] {
            tracy::SetThreadName("TecsTrace");
            ZoneScopedN("Tecs Trace");
            ecs::World().StartTrace();
            std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
            auto trace = ecs::World().StopTrace();

            for (auto &event : trace.transactionEvents) {
                std::stringstream ss;
                ss << event.thread;
                uint32_t thread_id;
                ss >> thread_id;
                trace.SetThreadName(tracy::GetThreadName(thread_id), event.thread);
            }

            std::ofstream traceFile("tecs-trace.csv");
            trace.SaveToCSV(traceFile);
            traceFile.close();

            Logf("Tecs performance trace complete");
            tracingStarted = false;
        }).detach();
    });
}
