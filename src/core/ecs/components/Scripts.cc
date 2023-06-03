#include "Scripts.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <atomic>

namespace ecs {
    template<>
    bool StructMetadata::Load<ScriptInstance>(ScriptInstance &instance, const picojson::value &src) {
        const auto &definitions = GetScriptDefinitions();
        const ScriptDefinition *definition = nullptr;
        if (!src.is<picojson::object>()) {
            Errorf("Script has invalid definition: %s", src.to_str());
            return false;
        }
        auto &srcObj = src.get<picojson::object>();
        for (auto param : srcObj) {
            if (param.first == "onTick") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = definitions.scripts.find(scriptName);
                    if (it != definitions.scripts.end()) {
                        if (definition) {
                            Errorf("Script has multiple definitions: %s", scriptName);
                            return false;
                        }
                        definition = &it->second;
                    } else {
                        Errorf("Script has unknown onTick definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script onTick has invalid definition: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "prefab") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = definitions.prefabs.find(scriptName);
                    if (it != definitions.prefabs.end()) {
                        if (definition) {
                            Errorf("Script has multiple definitions: %s", scriptName);
                            return false;
                        }
                        definition = &it->second;
                    } else {
                        Errorf("Script has unknown prefab definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script prefab has invalid definition: %s", param.second.to_str());
                    return false;
                }
            }
        }
        if (!definition) {
            Errorf("Script has no definition: %s", src.to_str());
            return false;
        }

        auto state = ScriptState(*definition);
        if (definition->context) {
            // Access will initialize default parameters
            void *dataPtr = state.definition.context->Access(state);
            Assertf(dataPtr, "Script definition returned null data: %s", state.definition.name);

            auto it = srcObj.find("parameters");
            if (it != srcObj.end()) {
                for (auto &field : state.definition.context->metadata.fields) {
                    if (!field.Load(dataPtr, it->second)) {
                        Errorf("Script %s has invalid parameter: %s", state.definition.name, field.name);
                        return false;
                    }
                }
            }
        }
        instance = std::make_shared<ScriptState>(std::move(state));
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
            if (std::holds_alternative<PrefabFunc>(state.definition.callback)) {
                obj["prefab"] = picojson::value(state.definition.name);
            } else {
                obj["onTick"] = picojson::value(state.definition.name);
            }

            if (state.definition.context) {
                std::lock_guard l(GetScriptManager().mutexes[state.definition.callback.index()]);

                const void *dataPtr = state.definition.context->Access(state);
                const void *defaultPtr = state.definition.context->GetDefault();
                Assertf(dataPtr, "Script definition returned null data: %s", state.definition.name);
                bool changed = false;
                for (auto &field : state.definition.context->metadata.fields) {
                    if (!field.Compare(dataPtr, defaultPtr)) {
                        changed = true;
                        break;
                    }
                }
                if (changed) {
                    for (auto &field : state.definition.context->metadata.fields) {
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

            if (oldState.definition.context) {
                const void *defaultPtr = oldState.definition.context->GetDefault();
                void *oldPtr = oldState.definition.context->Access(oldState);
                void *newPtr = newState->definition.context->Access(*newState);
                Assertf(oldPtr, "Script definition returned null data: %s", oldState.definition.name);
                Assertf(newPtr, "Script definition returned null data: %s", newState->definition.name);
                for (auto &field : oldState.definition.context->metadata.fields) {
                    field.Apply(newPtr, oldPtr, defaultPtr);
                    field.SetScope(newPtr, scope);
                }
            }
            dst.state = std::move(newState);
        }
    }

    bool ScriptState::operator==(const ScriptState &other) const {
        if (definition.name.empty() || !definition.context) return instanceId == other.instanceId;
        if (definition.name != other.definition.name) return false;
        if (definition.context != other.definition.context) return false;

        const void *aPtr = definition.context->Access(*this);
        const void *bPtr = definition.context->Access(other);
        Assertf(aPtr && bPtr, "Script definition returned null data: %s", definition.name);
        for (auto &field : definition.context->metadata.fields) {
            if (!field.Compare(aPtr, bPtr)) return false;
        }
        return true;
    }

    bool ScriptState::CompareOverride(const ScriptState &other) const {
        if (instanceId == other.instanceId) return true;
        if (definition.name != other.definition.name) return false;
        if (definition.callback.index() != other.definition.callback.index()) return false;
        if (std::get_if<PrefabFunc>(&definition.callback)) {
            if (definition.name == "gltf") {
                return GetParam<string>("model") == other.GetParam<string>("model");
            } else if (definition.name == "template") {
                return GetParam<string>("source") == other.GetParam<string>("source");
            }
        }
        return !definition.name.empty();
    }

    template<>
    void Component<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget) {
        if (liveTarget) {
            for (auto &instance : src.scripts) {
                if (!instance) continue;
                auto existing = std::find_if(dst.scripts.begin(), dst.scripts.end(), [&](auto &arg) {
                    return arg.GetInstanceId() == instance.GetInstanceId();
                });
                if (existing == dst.scripts.end()) {
                    dst.scripts.emplace_back(instance);
                }
            }
        } else {
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
    }

    const ScriptState *Scripts::FindScript(size_t instanceId) const {
        auto it = std::find_if(scripts.begin(), scripts.end(), [&](auto &arg) {
            return arg.GetInstanceId() == instanceId;
        });
        return it != scripts.end() ? it->state.get() : nullptr;
    }
} // namespace ecs
