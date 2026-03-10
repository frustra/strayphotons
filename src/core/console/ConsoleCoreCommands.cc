/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "console/Console.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalStructAccess.hh"
#include "ecs/components/Name.hh"
#include "strayphotons/cpp/Logging.hh"
#include "strayphotons/cpp/Utility.hh"

#include <shared_mutex>
#include <sstream>

template<typename Callback>
void mutateEntityTransform(const ecs::EntityRef &entityRef, Callback callback) {
    auto lock = ecs::StartTransaction<ecs::Write<ecs::TransformTree, ecs::TransformSnapshot>>();
    ecs::Entity entity = entityRef.Get(lock);
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

    funcs.Register<uint64_t, std::string>("wait",
        "Queue command for later (wait <ms> <command>)",
        [](uint64_t dt, std::string cmd) {
            GetConsoleManager().QueueParseAndExecute(cmd, chrono_clock::now() + std::chrono::milliseconds(dt));
        });

    funcs.Register<std::string, std::string>("toggle",
        "Toggle a CVar between values (toggle <cvar_name> [<value_a> <value_b>])",
        [this](std::string cvarName, std::string args) {
            std::shared_lock lock(cvarReadLock);

            auto cvarit = cvars.find(to_lower_copy(cvarName));
            if (cvarit != cvars.end()) {
                auto cvar = cvarit->second;
                if (cvar->IsValueType()) {
                    std::stringstream stream(args);
                    std::vector<std::string> values;
                    std::string value;
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
                transform.pose.RotateAxis(glm::radians(degrees), plane);
            });
        });

    funcs.Register<ecs::EntityRef, glm::vec3>("scale",
        "Scales an entity a relative amount (scale <entity> <x> <y> <z>)",
        [](ecs::EntityRef entityRef, glm::vec3 value) {
            mutateEntityTransform(entityRef, [&](auto lock, ecs::TransformTree &transform) {
                transform.pose.Scale(value);
            });
        });

    funcs.Register<std::string, double>("setsignal",
        "Set a signal value (setsignal <entity>/<signal> <value>)",
        [](std::string signalStr, double value) {
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

    funcs.Register<std::string, std::string>("togglesignal",
        "Toggle a signal between values (togglesignal <entity>/<signal> [<value_a> <value_b>])",
        [](std::string signalStr, std::string args) {
            ecs::SignalRef signal(signalStr);
            if (!signal) {
                Logf("Invalid signal string: %s", signalStr);
                return;
            }

            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Write<ecs::Signals>>();
            auto &entityRef = signal.GetEntity();
            ecs::Entity entity = entityRef.Get(lock);
            if (!entity.Exists(lock)) {
                Logf("Signal entity %s not found", entityRef.Name().String());
                return;
            }

            std::stringstream stream(args);
            std::vector<std::string> values;
            std::string value;
            while (stream >> value) {
                values.push_back(value);
            }

            auto signalValue = signal.GetSignal(lock);
            ToggleBetweenValues(signalValue, values.data(), values.size());
            signal.SetValue(lock, signalValue);
        });

    funcs.Register<std::string>("clearsignal",
        "Clear a signal value (clearsignal <entity>/<signal>)",
        [](std::string signalStr) {
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

    funcs.Register<std::string, std::string>("sendevent",
        "Send an entity an event (sendevent <entity>/<event> <value>)",
        [](std::string eventStr, std::string value) {
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
                    std::array<std::string, 5> values;
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

    funcs.Register<std::string, std::string>("setcomponent",
        "Set a data field on component (setcomponent <entity>#<component>.<field> <value>)",
        [](std::string componentStr, std::string exprStr) {
            ecs::SignalExpression expr(exprStr);
            if (!expr) {
                Errorf("Invalid value expression: %s", exprStr);
                return;
            }

            size_t delimiter = componentStr.find_first_of("#");
            if (delimiter >= componentStr.length()) {
                Errorf("Invalid entity component name, missing '#': %s", componentStr);
                return;
            }
            ecs::Name entityName = ecs::Name(componentStr.substr(0, delimiter), ecs::EntityScope());
            auto fieldPath = componentStr.substr(delimiter + 1);

            delimiter = fieldPath.find('.');
            if (delimiter == std::string::npos) {
                Errorf("Invalid component path, missing '.': %s", fieldPath);
                return;
            }
            auto componentName = fieldPath.substr(0, delimiter);

            auto comp = ecs::LookupComponent(componentName);
            if (!comp) {
                Errorf("Invalid component name: %s", componentName);
                return;
            }

            auto lock = ecs::QueueTransaction<ecs::WriteAll>([=](auto lock) {
                ecs::EntityRef ref(entityName);
                ecs::Entity ent = ref.Get(lock);
                if (!comp->HasComponent(lock, ent)) {
                    Warnf("Entity does not have component: %s", componentName);
                    return;
                }

                auto field = ecs::GetStructField(comp->metadata.type, fieldPath);
                if (!field) {
                    Errorf("Invalid component path, missing field: %s", fieldPath);
                    return;
                }

                auto signalValue = expr.Evaluate(lock);

                void *compPtr = comp->AccessMut(lock, ent);
                if (!compPtr) {
                    Errorf("Error, %s access returned null data: %s", componentName, ecs::ToString(lock, ent));
                    return;
                }
                ecs::WriteStructField(compPtr, *field, [&signalValue](double &value) {
                    value = signalValue;
                });
            });
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

            trace.SaveToCSV("tecs-trace.csv");

            Logf("Tecs performance trace complete");
            tracingStarted = false;
        }).detach();
    });
}
