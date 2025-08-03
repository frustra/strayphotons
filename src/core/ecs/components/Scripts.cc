/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Scripts.hh"

#include "assets/JsonHelpers.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"

#include <atomic>

namespace ecs {
    template<>
    bool StructMetadata::Load<ScriptInstance>(ScriptInstance &instance, const picojson::value &src) {
        const auto &definitions = GetScriptDefinitions();
        if (!src.is<picojson::object>()) {
            Errorf("Script has invalid definition: %s", src.to_str());
            return false;
        }
        const auto &srcObj = src.get<picojson::object>();
        auto nameIt = srcObj.find("name");
        if (nameIt == srcObj.end()) {
            Errorf("Script is missing name");
            return false;
        }
        const auto &scriptName = nameIt->second.get<std::string>();
        auto definitionIt = definitions.scripts.find(scriptName);
        if (definitionIt == definitions.scripts.end()) {
            Errorf("Script has unknown definition: %s", scriptName);
            return false;
        }

        auto newState = std::make_shared<ScriptState>(definitionIt->second);
        ScriptState &state = *newState;
        auto ctx = state.definition.context.lock();
        if (ctx) {
            auto it = srcObj.find("parameters");
            if (it != srcObj.end()) {
                // Access will initialize default parameters
                void *dataPtr = ctx->AccessMut(state);
                if (!dataPtr) {
                    Warnf("Can't set parameters on context-free script: %s", state.definition.name);
                } else {
                    for (auto &field : ctx->metadata.fields) {
                        if (!field.Load(dataPtr, it->second)) {
                            Errorf("Script %s has invalid parameter: %s", state.definition.name, field.name);
                            return false;
                        }
                    }
                }
            }
        }
        instance = newState;
        return true;
    }

    template<>
    void StructMetadata::Save<ScriptInstance>(const EntityScope &scope,
        picojson::value &dst,
        const ScriptInstance &src,
        const ScriptInstance *def) {
        if (!src.state) return;
        const auto &state = *src.state;
        if (state.definition.name.empty()) {
            dst = picojson::value("inline C++ lambda");
        } else if (!std::holds_alternative<std::monostate>(state.definition.callback)) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            auto &obj = dst.get<picojson::object>();
            obj["name"] = picojson::value(state.definition.name.str());

            auto ctx = state.definition.context.lock();
            if (ctx) {
                std::shared_lock l1(GetScriptManager().dynamicLibraryMutex);
                std::lock_guard l2(GetScriptManager().scripts[state.definition.type].mutex);

                const void *dataPtr = ctx->Access(state);
                if (!dataPtr) return;
                const void *defaultPtr = ctx->GetDefault();
                bool changed = false;
                for (auto &field : ctx->metadata.fields) {
                    if (!field.Compare(dataPtr, defaultPtr)) {
                        changed = true;
                        break;
                    }
                }
                if (changed) {
                    for (auto &field : ctx->metadata.fields) {
                        field.Save(scope, obj["parameters"], dataPtr, defaultPtr);
                    }
                }
            }
        }
    }

    template<>
    void StructMetadata::SetScope<ScriptInstance>(ScriptInstance &dst, const EntityScope &scope) {
        if (!dst.state) return;
        if (dst.state->scope != scope) {
            auto &oldState = *dst.state;
            // Create a new script instance so references to the old scope remain valid.
            auto newState = GetScriptManager().NewScriptInstance(scope, oldState.definition);

            auto oldCtx = oldState.definition.context.lock();
            auto newCtx = newState->definition.context.lock();
            if (oldCtx) {
                const void *defaultPtr = oldCtx->GetDefault();
                const void *oldPtr = oldCtx->Access(oldState);
                if (oldPtr) {
                    void *newPtr = newCtx->AccessMut(*newState);
                    Assertf(newPtr, "Script definition returned null data: %s", newState->definition.name);
                    for (auto &field : oldCtx->metadata.fields) {
                        field.Apply(newPtr, oldPtr, defaultPtr);
                        field.SetScope(newPtr, scope);
                    }
                }
            }
            dst.state = std::move(newState);
        }
    }

    ScriptState &Scripts::AddScript(const EntityScope &scope, const ScriptDefinition &definition) {
        auto &instance = scripts.emplace_back(scope, definition);
        return *(instance.state);
    }

    ScriptState &Scripts::AddScript(const EntityScope &scope, const std::string &scriptName) {
        return AddScript(scope, GetScriptDefinitions().scripts.at(scriptName));
    }

    bool ScriptState::operator==(const ScriptState &other) const {
        if (definition.name.empty()) return instanceId == other.instanceId;
        if (definition.name != other.definition.name) return false;
        // Compare (definition.context != other.definition.context) without locking both pointers
        if (definition.context.owner_before(other.definition.context) ||
            other.definition.context.owner_before(definition.context)) {
            return false;
        }

        auto ctx = definition.context.lock();
        if (!ctx) return instanceId == other.instanceId;
        const void *aPtr = ctx->Access(*this);
        const void *bPtr = ctx->Access(other);
        if (aPtr == bPtr) return true;
        if (!aPtr || !bPtr) return false;
        for (auto &field : ctx->metadata.fields) {
            if (!field.Compare(aPtr, bPtr)) return false;
        }
        return true;
    }

    bool ScriptState::CompareOverride(const ScriptState &other) const {
        if (instanceId == other.instanceId) return true;
        if (definition.name != other.definition.name) return false;
        if (definition.callback.index() != other.definition.callback.index()) return false;
        if (std::holds_alternative<PrefabFunc>(definition.callback)) {
            if (definition.name == "prefab_gltf") {
                return GetParam<string>("model") == other.GetParam<string>("model");
            } else if (definition.name == "prefab_template") {
                return GetParam<string>("source") == other.GetParam<string>("source");
            }
        }
        return !definition.name.empty();
    }

    template<>
    void EntityComponent<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget) {
        for (auto &instance : src.scripts) {
            if (!instance) continue;
            auto existing = std::find_if(dst.scripts.begin(), dst.scripts.end(), [&](auto &arg) {
                return instance.CompareOverride(arg);
            });
            if (existing == dst.scripts.end()) {
                dst.scripts.emplace_back(instance);
            } else if (liveTarget && existing->GetInstanceId() != instance.GetInstanceId()) {
                *existing = instance;
            }
        }
    }

    const ScriptState *Scripts::FindScript(size_t instanceId) const {
        auto it = std::find_if(scripts.begin(), scripts.end(), [&](auto &arg) {
            return arg.GetInstanceId() == instanceId;
        });
        return it != scripts.end() ? it->state.get() : nullptr;
    }
} // namespace ecs
