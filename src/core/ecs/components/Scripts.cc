#include "Scripts.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <atomic>
#include <utility>

namespace ecs {
    ScriptDefinitions &GetScriptDefinitions() {
        static ScriptDefinitions scriptDefinitions;
        return scriptDefinitions;
    }

    void ScriptDefinitions::RegisterScript(ScriptDefinition &&definition) {
        Assertf(!scripts.contains(definition.name), "Script definition already exists: %s", definition.name);
        scripts.emplace(definition.name, definition);
    }

    void ScriptDefinitions::RegisterPrefab(ScriptDefinition &&definition) {
        Assertf(!prefabs.contains(definition.name), "Prefab definition already exists: %s", definition.name);
        prefabs.emplace(definition.name, definition);
    }

    static std::atomic_size_t nextInstanceId;

    ScriptState::ScriptState() : instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, const ScriptDefinition &definition)
        : scope(scope), definition(definition), instanceId(++nextInstanceId) {}

    template<>
    bool StructMetadata::Load<ScriptInstance>(const EntityScope &scope,
        ScriptInstance &instance,
        const picojson::value &src) {
        const auto &definitions = GetScriptDefinitions();
        const ScriptDefinition *definition = nullptr;
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
        } else if (definition->context) {
            instance = ScriptInstance(scope, *definition);
            auto &state = *instance.state;
            // Access will initialize default parameters
            void *dataPtr = state.definition.context->Access(state);
            Assertf(dataPtr, "Script definition returned null data: %s", state.definition.name);

            auto it = srcObj.find("parameters");
            if (it != srcObj.end()) {
                for (auto &field : state.definition.context->metadata.fields) {
                    if (!field.Load(scope, dataPtr, it->second)) {
                        Errorf("Script %s has invalid parameter: %s", state.definition.name, field.name);
                        return false;
                    }
                }
            }
        }
        return true;
    }

    template<>
    void StructMetadata::Save<ScriptInstance>(const EntityScope &scope,
        picojson::value &dst,
        const ScriptInstance &src,
        const ScriptInstance &def) {
        if (!src.state) return;
        auto &srcState = *src.state;
        if (srcState.definition.name.empty()) {
            dst = picojson::value("inline C++ lambda");
        } else if (!std::holds_alternative<std::monostate>(srcState.definition.callback)) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            auto &obj = dst.get<picojson::object>();
            if (std::holds_alternative<PrefabFunc>(srcState.definition.callback)) {
                obj["prefab"] = picojson::value(srcState.definition.name);
            } else {
                obj["onTick"] = picojson::value(srcState.definition.name);
            }

            if (srcState.definition.context) {
                const void *dataPtr = srcState.definition.context->Access(srcState);
                const void *defaultPtr = srcState.definition.context->GetDefault();
                Assertf(dataPtr, "Script definition returned null data: %s", srcState.definition.name);
                bool changed = false;
                for (auto &field : srcState.definition.context->metadata.fields) {
                    if (!field.Compare(dataPtr, defaultPtr)) {
                        changed = true;
                        break;
                    }
                }
                if (changed) {
                    for (auto &field : srcState.definition.context->metadata.fields) {
                        field.Save(scope, obj["parameters"], dataPtr, defaultPtr);
                    }
                }
            }
        }
    }

    template<>
    void StructMetadata::Save<Scripts>(const EntityScope &scope,
        picojson::value &dst,
        const Scripts &src,
        const Scripts &def) {
        picojson::array arrayOut;
        for (auto &instance : src.scripts) {
            // Skip if the script is the same as the default
            if (!instance || sp::contains(def.scripts, instance)) continue;

            picojson::value val;
            sp::json::Save(scope, val, *instance.state);
            arrayOut.emplace_back(val);
        }
        if (arrayOut.size() > 1) {
            dst = picojson::value(arrayOut);
        } else if (arrayOut.size() == 1) {
            dst = arrayOut.front();
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
        for (auto &instance : src.scripts) {
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

    void Scripts::Init(Lock<Read<Name, Scripts>, Write<EventInput>> lock, const Entity &ent) {
        if (!ent.Has<Scripts>(lock)) return;
        for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
            if (!instance) continue;
            auto &state = *instance.state;

            std::lock_guard l(state.mutex);
            if (!state.eventQueue) state.eventQueue = NewEventQueue();
            if (state.definition.initFunc) (*state.definition.initFunc)(state);
            if (ent.Has<EventInput>(lock)) {
                auto &eventInput = ent.Get<EventInput>(lock);
                for (auto &event : state.definition.events) {
                    eventInput.Register(lock, state.eventQueue, event);
                }
            } else if (!state.definition.events.empty()) {
                Warnf("Script %s has events but %s has no EventInput component",
                    state.definition.name,
                    ecs::ToString(lock, ent));
            }
        }
    }

    void Scripts::OnTickRoot(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval) {
        for (auto &instance : scripts) {
            if (!instance) continue;
            auto &state = *instance.state;
            auto callback = std::get_if<OnTickRootFunc>(&state.definition.callback);
            if (callback && *callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnTickRoot");
                ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Scripts::OnTickEntity(EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
        for (auto &instance : scripts) {
            if (!instance) continue;
            auto &state = *instance.state;
            std::lock_guard l(state.mutex);
            auto callback = std::get_if<OnTickEntityFunc>(&state.definition.callback);
            if (callback && *callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnTickEntity");
                // ZoneStr(ecs::ToString(entLock));
                (*callback)(state, entLock, interval);
            }
        }
    }

    void Scripts::OnPhysicsUpdate(EntityLock<PhysicsUpdateLock> entLock, chrono_clock::duration interval) {
        for (auto &instance : scripts) {
            if (!instance) continue;
            auto &state = *instance.state;
            std::lock_guard l(state.mutex);
            auto callback = std::get_if<OnPhysicsUpdateFunc>(&state.definition.callback);
            if (callback && *callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                // ZoneScopedN("OnPhysicsUpdate");
                // ZoneStr(ecs::ToString(entLock));
                (*callback)(state, entLock, interval);
            }
        }
    }

    void Scripts::RunPrefabs(Lock<AddRemove> lock, Entity ent) {
        if (!ent.Has<Scripts, SceneInfo>(lock)) return;

        ZoneScopedN("RunPrefabs");
        ZoneStr(ecs::ToString(lock, ent));

        auto scene = ent.Get<const SceneInfo>(lock).scene;
        Assertf(scene, "RunPrefabs entity has null scene: %s", ecs::ToString(lock, ent));

        // Prefab scripts may add additional scripts while iterating.
        // The Scripts component may not remain valid if storage is resized,
        // so we need to reference the lock every loop iteration.
        for (size_t i = 0; i < ent.Get<const Scripts>(lock).scripts.size(); i++) {
            auto instance = ent.Get<const Scripts>(lock).scripts[i];
            if (!instance) continue;
            auto &state = *instance.state;
            std::lock_guard l(state.mutex);
            auto callback = std::get_if<PrefabFunc>(&state.definition.callback);
            if (callback && *callback) (*callback)(state, scene, lock, ent);
        }
    }

    const ScriptState *Scripts::FindScript(size_t instanceId) const {
        auto it = std::find_if(scripts.begin(), scripts.end(), [&](auto &arg) {
            return arg.GetInstanceId() == instanceId;
        });
        return it != scripts.end() ? it->state.get() : nullptr;
    }
} // namespace ecs
