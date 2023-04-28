#include "Scripts.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <atomic>

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

    std::array<std::recursive_mutex, std::variant_size_v<ScriptCallback>> ScriptTypeMutex;

    ScriptState::ScriptState() : instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const ScriptState &other)
        : scope(other.scope), definition(other.definition), eventQueue(other.eventQueue), userData(other.userData),
          instanceId(other.instanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, const ScriptDefinition &definition)
        : scope(scope), definition(definition), instanceId(++nextInstanceId) {}

    template<>
    bool StructMetadata::Load<ScriptInstance>(const EntityScope &scope,
        ScriptInstance &instance,
        const picojson::value &src) {
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
        } else if (definition->context) {
            instance = ScriptInstance(scope, *definition);
            auto &state = *instance.state;
            std::lock_guard l(ScriptTypeMutex[state.definition.callback.index()]);
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
        auto &state = *src.state;
        std::lock_guard l(ScriptTypeMutex[state.definition.callback.index()]);
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
    bool StructMetadata::Load<Scripts>(const EntityScope &scope, Scripts &dst, const picojson::value &src) {
        // Scripts will already be populated by subfield-handler, just update the callback mask
        dst.callbackMask.reset();
        for (auto &script : dst.scripts) {
            if (script) dst.callbackMask.set(script.state->definition.callback.index());
        }
        return true;
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
            sp::json::Save(scope, val, instance);
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

    ScriptInstance ScriptInstance::Copy() const {
        Assert(state, "ScriptInstance::Copy called on empty instance");
        ScriptInstance newInstance = *this;
        newInstance.state = std::make_shared<ScriptState>(*state);
        return newInstance;
    }

    template<>
    void Component<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget) {
        for (auto &instance : src.scripts) {
            if (!instance) continue;
            auto existing = std::find_if(dst.scripts.begin(), dst.scripts.end(), [&](auto &arg) {
                return instance.CompareOverride(arg);
            });
            if (existing == dst.scripts.end()) {
                dst.scripts.emplace_back(instance.Copy());
            } else if (liveTarget && existing->GetInstanceId() != instance.GetInstanceId()) {
                *existing = instance.Copy();
            }
        }
        dst.callbackMask.reset();
        for (auto &script : dst.scripts) {
            if (script) dst.callbackMask.set(script.state->definition.callback.index());
        }
    }

    void Scripts::Init(Lock<Read<Name, Scripts>, Write<EventInput>> lock, const Entity &ent) {
        if (!ent.Has<Scripts>(lock)) return;
        ZoneScoped;
        for (auto &instance : ent.Get<const Scripts>(lock).scripts) {
            if (!instance) continue;
            auto &state = *instance.state;

            std::lock_guard l(ScriptTypeMutex[state.definition.callback.index()]);
            if (!state.eventQueue) {
                {
                    ZoneScopedN("InitEventQueue");
                    state.eventQueue = NewEventQueue();
                }
                {
                    ZoneScopedN("InitFunc");
                    if (state.definition.initFunc) (*state.definition.initFunc)(state);
                }
                if (ent.Has<EventInput>(lock)) {
                    ZoneScopedN("RegisterEvents");
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
    }

    void Scripts::OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval) const {
        ZoneScoped;
        for (auto &instance : scripts) {
            if (!instance) continue;
            auto &state = *instance.state;
            auto callback = std::get_if<OnTickFunc>(&state.definition.callback);
            if (callback && *callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnTick");
                // ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Scripts::OnPhysicsUpdate(PhysicsUpdateLock lock, const Entity &ent, chrono_clock::duration interval) const {
        ZoneScoped;
        for (auto &instance : scripts) {
            if (!instance) continue;
            auto &state = *instance.state;
            auto callback = std::get_if<OnPhysicsUpdateFunc>(&state.definition.callback);
            if (callback && *callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnPhysicsUpdate");
                ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Scripts::RunPrefabs(Lock<AddRemove> lock, Entity ent) {
        if (!ent.Has<Scripts, SceneInfo>(lock)) return;
        if (!ent.Get<const Scripts>(lock).HasPrefab()) return;

        ZoneScopedN("RunPrefabs");
        ZoneStr(ecs::ToString(lock, ent));

        auto scene = ent.Get<const SceneInfo>(lock).scene;
        Assertf(scene, "RunPrefabs entity has null scene: %s", ecs::ToString(lock, ent));

        std::lock_guard l(ScriptTypeMutex[ScriptCallbackIndex<PrefabFunc>()]);
        // Prefab scripts may add additional scripts while iterating.
        // The Scripts component may not remain valid if storage is resized,
        // so we need to reference the lock every loop iteration.
        for (size_t i = 0; i < ent.Get<const Scripts>(lock).scripts.size(); i++) {
            auto instance = ent.Get<const Scripts>(lock).scripts[i];
            if (!instance) continue;
            auto &state = *instance.state;
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
