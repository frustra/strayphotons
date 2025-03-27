/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
#include "ecs/Components.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"
#include "game/SceneImpl.hh"
#include "game/SceneManager.hh"

#include <fstream>
#include <picojson/picojson.h>

namespace sp {
    using namespace ecs;

    bool ComponentOrderFunc(const std::string &a, const std::string &b) {
        // Sort component keys in the order they are defined in the ECS
        return GetComponentIndex(a) < GetComponentIndex(b);
    }

    template<typename T, typename... AllComponentTypes, template<typename...> typename ECSType>
    std::optional<T> BuildFlatComponent(const Tecs::Lock<ECSType<AllComponentTypes...>, ReadAll> &staging,
        const Entity &e) {
        if constexpr (std::is_same_v<T, Name> || std::is_same_v<T, SceneInfo>) {
            if (e.Has<T>(staging)) {
                return e.Get<T>(staging);
            } else {
                return {};
            }
        } else if constexpr (std::is_same_v<T, TransformSnapshot>) {
            // Ignore, this is handled by TransformTree
            return {};
        } else {
            std::optional<T> flatComp = {};
            auto stagingId = e;
            while (stagingId.Has<SceneInfo>(staging)) {
                auto &stagingInfo = stagingId.Get<SceneInfo>(staging);

                if constexpr (std::is_same_v<T, SceneProperties>) {
                    if (!flatComp) {
                        flatComp = LookupComponent<SceneProperties>().StagingDefault();
                    }

                    Assertf(stagingInfo.scene, "Staging entity %s has null scene", ToString(staging, stagingId));
                    auto properties = stagingInfo.scene.data->GetProperties(staging);
                    properties.fixedGravity = properties.rootTransform * glm::vec4(properties.fixedGravity, 0.0f);
                    properties.gravityTransform = properties.rootTransform * properties.gravityTransform;
                    LookupComponent<SceneProperties>().ApplyComponent(*flatComp, properties, false);
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    if (stagingId.Has<T>(staging)) {
                        if (!flatComp) {
                            const ComponentBase *comp = LookupComponent(typeid(T));
                            Assertf(comp, "Couldn't lookup component type: %s", typeid(T).name());
                            flatComp = comp->GetStagingDefault<T>();
                        }

                        if constexpr (std::is_same_v<T, TransformTree> || std::is_same_v<T, Animation>) {
                            auto transform = stagingId.Get<TransformTree>(staging);

                            if constexpr (std::is_same_v<T, TransformTree>) {
                                // Apply scene root transform
                                if (!transform.parent) {
                                    Assertf(stagingInfo.scene,
                                        "Staging entity %s has null scene",
                                        ToString(staging, stagingId));
                                    auto &properties = stagingInfo.scene.data->GetProperties(staging);
                                    if (properties.rootTransform != Transform()) {
                                        transform.pose = properties.rootTransform * transform.pose.Get();
                                    }
                                }

                                LookupComponent<TransformTree>().ApplyComponent(*flatComp, transform, false);
                            } else {
                                auto animation = stagingId.Get<Animation>(staging);

                                // Apply scene root transform
                                if (!transform.parent) {
                                    Assertf(stagingInfo.scene,
                                        "Staging entity %s has null scene",
                                        ToString(staging, stagingId));
                                    auto &properties = stagingInfo.scene.data->GetProperties(staging);
                                    if (properties.rootTransform != Transform()) {
                                        for (auto &state : animation.states) {
                                            state.pos = properties.rootTransform * glm::vec4(state.pos, 1);
                                        }
                                    }
                                }

                                LookupComponent<Animation>().ApplyComponent(*flatComp, animation, false);
                            }
                        } else {
                            auto &srcComp = stagingId.Get<T>(staging);
                            LookupComponent<T>().ApplyComponent(*flatComp, srcComp, false);
                        }
                    }
                }

                stagingId = stagingInfo.nextStagingId;
            }

            return flatComp;
        }
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    bool SaveEntityIfChanged(const Tecs::Lock<ECSType<AllComponentTypes...>, ReadAll> &live,
        const Lock<ReadAll> &staging,
        const EntityScope &scope,
        picojson::value &dst,
        const Entity &src) {
        Assertf(IsLive(src), "SaveEntityIfChanged expected source entity to be from live ECS.");
        picojson::object components(ComponentOrderFunc);
        ( // For each component:
            [&] {
                using T = AllComponentTypes;

                if constexpr (std::is_same_v<T, Name> || std::is_same_v<T, SceneInfo>) {
                    // Skip
                } else if constexpr (std::is_same_v<T, SceneProperties> || std::is_same_v<T, TransformSnapshot>) {
                    // Skip
                } else if constexpr (std::is_same_v<T, Signals>) {
                    // Convert live signals to SignalOutput / SignalBindings for saving
                    auto signals = GetSignalManager().GetSignals(src);
                    if (!signals.empty()) {
                        auto &sceneInfo = src.Get<SceneInfo>(live);
                        std::optional<SignalOutput> liveOutputs;
                        std::optional<SignalBindings> liveBindings;
                        for (auto &signalRef : signals) {
                            if (signalRef.HasValue(live)) {
                                if (!liveOutputs) liveOutputs = SignalOutput();
                                liveOutputs->signals[signalRef.GetSignalName()] = signalRef.GetValue(live);
                            }
                            if (signalRef.HasBinding(live)) {
                                if (!liveBindings) liveBindings = SignalBindings();
                                liveBindings->bindings[signalRef.GetSignalName()] = signalRef.GetBinding(live);
                            }
                        }
                        auto stagingOutputs = BuildFlatComponent<SignalOutput>(staging, sceneInfo.rootStagingId);
                        auto stagingBindings = BuildFlatComponent<SignalBindings>(staging, sceneInfo.rootStagingId);

                        if (liveOutputs) {
                            const Component<SignalOutput> &base = LookupComponent<SignalOutput>();
                            auto &value = components[base.name];
                            SignalOutput *outputsPtr = nullptr;
                            if (stagingOutputs) outputsPtr = &stagingOutputs.value();
                            for (const StructField &field : base.metadata.fields) {
                                field.Save(scope, value, &liveOutputs.value(), outputsPtr);
                            }
                            StructMetadata::Save<SignalOutput>(scope, value, liveOutputs.value(), outputsPtr);
                            if (value.is<picojson::null>()) {
                                components.erase(base.name);
                            }
                        }
                        if (liveBindings) {
                            const Component<SignalBindings> &base = LookupComponent<SignalBindings>();
                            auto &value = components[base.name];
                            SignalBindings *bindingsPtr = nullptr;
                            if (stagingBindings) bindingsPtr = &stagingBindings.value();
                            for (const StructField &field : base.metadata.fields) {
                                field.Save(scope, value, &liveBindings.value(), bindingsPtr);
                            }
                            StructMetadata::Save<SignalBindings>(scope, value, liveBindings.value(), bindingsPtr);
                            if (value.is<picojson::null>()) {
                                components.erase(base.name);
                            }
                        }
                    }
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    const Component<T> &base = LookupComponent<T>();
                    if (src.Has<T>(live)) {
                        picojson::value &value = components[base.name];
                        auto liveComp = src.Get<T>(live);
                        auto &sceneInfo = src.Get<SceneInfo>(live);
                        std::optional<T> stagingComp = BuildFlatComponent<T>(staging, sceneInfo.rootStagingId);
                        T *compPtr = nullptr;
                        T flatComp = {};
                        if (stagingComp) {
                            base.ApplyComponent(flatComp, *stagingComp, true);
                            compPtr = &flatComp;
                        }
                        if constexpr (std::is_same_v<T, TransformTree>) {
                            // Special case due to transform override logic
                            if (liveComp.pose != flatComp.pose || liveComp.parent != flatComp.parent) {
                                for (const StructField &field : base.metadata.fields) {
                                    field.Save(scope, value, &liveComp, &base.StagingDefault());
                                }
                                StructMetadata::Save<T>(scope, value, liveComp, &base.StagingDefault());
                            } else {
                                components.erase(base.name);
                            }
                        } else {
                            for (const StructField &field : base.metadata.fields) {
                                field.Save(scope, value, &liveComp, compPtr);
                            }
                            StructMetadata::Save<T>(scope, value, liveComp, compPtr);
                            if (value.is<picojson::null>()) {
                                components.erase(base.name);
                            }
                        }
                    }
                }
            }(),
            ...);
        if (!components.empty()) {
            if (src.Has<Name>(live)) {
                auto &name = src.Get<Name>(live);
                json::Save(scope, components["name"], name);
            }
            dst = picojson::value(components);
            return true;
        } else {
            dst = picojson::value();
            return false;
        }
    }

    void SceneManager::SaveSceneJson(const std::string &sceneName) {
        auto scene = stagedScenes.Load(sceneName);
        if (scene) {
            Tracef("Saving staging scene: %s", scene->data->path);
            auto staging = StartStagingTransaction<ReadAll>();

            EntityScope scope(scene->data->name, "");

            picojson::array entities;
            for (auto &e : staging.EntitiesWith<SceneInfo>()) {
                if (!e.Has<SceneInfo>(staging)) continue;
                auto &sceneInfo = e.Get<SceneInfo>(staging);
                // Skip entities that aren't part of this scene, or were created by a prefab script
                if (sceneInfo.scene != scene || sceneInfo.prefabStagingId) continue;

                picojson::object components(ComponentOrderFunc);
                if (e.Has<Name>(staging)) {
                    auto &name = e.Get<Name>(staging);
                    if (scene->ValidateEntityName(name)) {
                        json::Save(scope, components["name"], name);
                    }
                }
                ForEachComponent([&](const std::string &name, const ComponentBase &comp) {
                    if (name == "scene_properties") return;
                    if (comp.HasComponent(staging, e)) {
                        auto &value = components[comp.name];
                        if (comp.metadata.fields.empty() || value.is<picojson::null>()) {
                            value.set<picojson::object>({});
                        }
                        comp.SaveEntity(staging, scope, value, e);
                    }
                });
                entities.emplace_back(components);
            }

            static auto sceneOrderFunc = [](const std::string &a, const std::string &b) {
                // Force "entities" to be sorted last
                if (b == "entities") {
                    return a < "zentities";
                } else if (a == "entities") {
                    return "zentities" < b;
                } else {
                    return a < b;
                }
            };
            picojson::object sceneObj(sceneOrderFunc);
            static const SceneProperties defaultProperties = {};
            static const ScenePriority defaultPriority = ScenePriority::Scene;
            json::SaveIfChanged(scope, sceneObj, "properties", scene->data->GetProperties(staging), &defaultProperties);
            json::SaveIfChanged(scope, sceneObj, "priority", scene->data->priority, &defaultPriority);
            sceneObj["entities"] = picojson::value(entities);
            auto val = picojson::value(sceneObj);
            auto scenePath = scene->asset->path;
            Logf("Saving scene %s to '%s'", scene->data->name, scenePath.string());

            std::ofstream out;
            if (Assets().OutputStream(scenePath.string(), out)) {
                auto outputJson = val.serialize(true);
                out.write(outputJson.c_str(), outputJson.size());
                out.close();
            }
        } else {
            Errorf("SceneManager::SaveSceneJson: scene %s not found", sceneName);
        }
    }

    void SceneManager::SaveLiveSceneJson(const std::string &outputPath) {
        Tracef("Saving live scene to: %s", outputPath);
        auto staging = StartStagingTransaction<ReadAll>();
        auto live = StartTransaction<ReadAll>();

        EntityScope scope;
        auto delim = outputPath.rfind('/');
        if (delim != std::string::npos) {
            scope.scene = outputPath.substr(delim + 1);
        } else if (outputPath.empty()) {
            scope.scene = "save0";
        } else {
            scope.scene = outputPath;
        }

        picojson::array entities;
        for (auto &e : live.EntitiesWith<SceneInfo>()) {
            if (!e.Has<SceneInfo>(live)) continue;

            if (e == sp::entities::Spawn) {
                // Replace spawn point with player position
                picojson::value ent;
                SaveEntityIfChanged(live, staging, scope, ent, e);
                if (!ent.is<picojson::object>()) {
                    picojson::object spawnObj;
                    spawnObj["name"] = picojson::value(sp::entities::Spawn.Name().String());
                    ent = picojson::value(spawnObj);
                }
                auto &spawnObj = ent.get<picojson::object>();

                auto &transform = sp::entities::Direction.GetLive().Get<TransformSnapshot>(live).globalPose;
                // TODO: Save entities::Head rotation to a new SpawnLook entity
                json::Save(scope, spawnObj["transform"], transform);
                entities.emplace_back(ent);
            } else {
                picojson::value ent;
                if (SaveEntityIfChanged(live, staging, scope, ent, e)) {
                    entities.emplace_back(ent);
                }
            }
        }
        // Generate scene_connection entity
        picojson::object connections;
        for (auto &scene : scenes[SceneType::Async]) {
            if (!scene || !scene->data || scene->data->priority == ScenePriority::SaveGame) continue;
            // TODO: Add Async scenes with an on-init condition (timer? load-once flag?)
            connections[scene->data->name] = picojson::value("1");
        }
        for (auto &scene : scenes[SceneType::World]) {
            if (!scene || !scene->data || scene->data->priority == ScenePriority::SaveGame) continue;
            connections[scene->data->name] = picojson::value("1");
        }
        if (!connections.empty()) {
            picojson::object ent;
            ent["scene_connection"] = picojson::value(connections);
            entities.emplace_back(ent);
        }

        static auto sceneOrderFunc = [](const std::string &a, const std::string &b) {
            // Force "entities" to be sorted last
            if (b == "entities") {
                return a < "zentities";
            } else if (a == "entities") {
                return "zentities" < b;
            } else {
                return a < b;
            }
        };
        picojson::object sceneObj(sceneOrderFunc);
        // TODO: Set up scene connections
        // static const SceneProperties defaultProperties = {};
        // json::SaveIfChanged(scope, sceneObj, "properties", scene->data->GetProperties(staging), &defaultProperties);
        json::Save(scope, sceneObj["priority"], ScenePriority::SaveGame);
        sceneObj["entities"] = picojson::value(entities);
        auto val = picojson::value(sceneObj);
        auto scenePath = std::string(outputPath.empty() ? "save0" : outputPath) + ".json";
        Logf("Saving live scene to '%s'", scenePath);

        std::ofstream out;
        if (Assets().OutputStream(scenePath, out)) {
            auto outputJson = val.serialize(true);
            out.write(outputJson.c_str(), outputJson.size());
            out.close();
        }
    }
} // namespace sp
