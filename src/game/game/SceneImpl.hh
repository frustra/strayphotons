#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalManager.hh"

#include <bitset>

namespace sp::scene {
    using namespace ecs;

    // Build a flattened set of components from the staging ECS.
    // The result includes staging components from the provided entity and all lower priority entities.
    // Transform components will have their scene root transforms applied to their poses.
    // Scripts will be initialized and their event queues will be created.
    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    FlatEntity BuildEntity(const Tecs::Lock<ECSType<AllComponentTypes...>, ReadAll> &staging, const Entity &e) {
        ZoneScoped;
        FlatEntity flatEntity;
        if (!e.Has<SceneInfo>(staging)) return flatEntity;

        if (e.Has<Name>(staging)) {
            auto &name = std::get<std::optional<Name>>(flatEntity);
            name = e.Get<Name>(staging);
        }

        auto stagingId = e;
        while (stagingId.Has<SceneInfo>(staging)) {
            auto &stagingInfo = stagingId.Get<SceneInfo>(staging);

            ( // For each component:
                [&] {
                    using T = AllComponentTypes;

                    if constexpr (std::is_same_v<T, Name>) {
                        // Ignore, this is handled above
                    } else if constexpr (std::is_same_v<T, SceneInfo>) {
                        // Ignore, this is handled by the scene
                    } else if constexpr (std::is_same_v<T, TransformSnapshot>) {
                        // Ignore, this is handled by TransformTree
                    } else if constexpr (std::is_same_v<T, Animation>) {
                        // Ignore, this is handled bellow and depends on the final TransformTree
                    } else if constexpr (std::is_same_v<T, SceneProperties>) {
                        auto &component = std::get<std::optional<SceneProperties>>(flatEntity);
                        if (!component) {
                            component = LookupComponent<SceneProperties>().StagingDefault();
                        }

                        Assertf(stagingInfo.scene, "Staging entity %s has null scene", ToString(staging, stagingId));
                        auto properties = stagingInfo.scene.data->GetProperties(staging);
                        properties.fixedGravity = properties.rootTransform * glm::vec4(properties.fixedGravity, 0.0f);
                        properties.gravityTransform = properties.rootTransform * properties.gravityTransform;
                        LookupComponent<SceneProperties>().ApplyComponent(component.value(), properties, false);
                    } else if constexpr (!Tecs::is_global_component<T>()) {
                        if (!stagingId.Has<T>(staging)) return;

                        auto &component = std::get<std::optional<T>>(flatEntity);
                        if (!component) {
                            const ecs::ComponentBase *comp = LookupComponent(typeid(T));
                            Assertf(comp, "Couldn't lookup component type: %s", typeid(T).name());
                            component = comp->GetStagingDefault<T>();
                        }

                        if constexpr (std::is_same_v<T, TransformTree>) {
                            auto transform = stagingId.Get<TransformTree>(staging);

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

                            LookupComponent<TransformTree>().ApplyComponent(component.value(), transform, false);
                        } else if constexpr (std::is_same_v<T, Scripts>) {
                            auto scripts = stagingId.Get<Scripts>(staging);

                            // Create a new script instance for each staging definition
                            for (auto &script : scripts.scripts) {
                                if (!script.state) continue;
                                script.state = GetScriptManager().NewScriptInstance(*script.state);
                            }

                            LookupComponent<Scripts>().ApplyComponent(component.value(), scripts, false);
                        } else {
                            auto &srcComp = stagingId.Get<T>(staging);
                            LookupComponent<T>().ApplyComponent(component.value(), srcComp, false);
                        }
                    }
                }(),
                ...);

            stagingId = stagingInfo.nextStagingId;
        }

        auto &flatTransform = std::get<std::optional<TransformTree>>(flatEntity);
        auto &flatAnimation = std::get<std::optional<Animation>>(flatEntity);
        if (flatTransform) {
            ZoneScopedN("ApplySceneTransform");
            stagingId = e;
            while (stagingId.Has<SceneInfo>(staging)) {
                auto &stagingInfo = stagingId.Get<SceneInfo>(staging);
                if (stagingId.Has<Animation>(staging)) {
                    auto animation = stagingId.Get<Animation>(staging);

                    // Apply scene root transform
                    if (!flatTransform->parent) {
                        Assertf(stagingInfo.scene, "Staging entity %s has null scene", ToString(staging, stagingId));
                        auto &properties = stagingInfo.scene.data->GetProperties(staging);
                        if (properties.rootTransform != Transform()) {
                            for (auto &state : animation.states) {
                                state.pos = properties.rootTransform * glm::vec4(state.pos, 1);
                            }
                        }
                    }

                    if (!flatAnimation) {
                        flatAnimation = LookupComponent<Animation>().StagingDefault();
                    }
                    LookupComponent<Animation>().ApplyComponent(flatAnimation.value(), animation, false);
                }
                stagingId = stagingInfo.nextStagingId;
            }
        }

        return flatEntity;
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void ApplyFlatEntity(const Tecs::Lock<ECSType<AllComponentTypes...>, AddRemove> &live,
        const Entity &liveId,
        const FlatEntity &flatEntity,
        bool resetLive) {
        ZoneScoped;

        // Apply flattened staging components to the live id, and remove any components that are no longer in staging
        ( // For each component:
            [&] {
                using T = AllComponentTypes;
                if constexpr (std::is_same_v<T, Name>) {
                    auto &name = std::get<std::optional<Name>>(flatEntity);
                    if (name) liveId.Set<Name>(live, *name);
                } else if constexpr (std::is_same_v<T, SceneInfo>) {
                    // Ignore, this should always be set
                } else if constexpr (std::is_same_v<T, SignalOutput>) {
                    // Skip, this is handled bellow
                } else if constexpr (std::is_same_v<T, SignalBindings>) {
                    // Skip, this is handled bellow
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    auto &component = std::get<std::optional<T>>(flatEntity);
                    if (component) {
                        if (resetLive) liveId.Unset<T>(live);

                        auto &dstComp = liveId.Get<T>(live);
                        LookupComponent<T>().ApplyComponent(dstComp, *component, true);
                    } else if (liveId.Has<T>(live)) {
                        if constexpr (std::is_same_v<T, TransformSnapshot>) {
                            auto &transformOpt = std::get<std::optional<TransformTree>>(flatEntity);
                            if (!transformOpt) {
                                liveId.Unset<TransformSnapshot>(live);
                            }
                        } else {
                            liveId.Unset<T>(live);
                        }
                    }
                }
            }(),
            ...);

        if (resetLive) {
            GetSignalManager().ClearEntity(live, liveId);
        }

        auto &signalOutput = std::get<std::optional<SignalOutput>>(flatEntity);
        if (signalOutput) {
            for (auto &[signalName, value] : signalOutput.value().signals) {
                if (!std::isfinite(value)) continue;
                SignalRef ref(liveId, signalName);
                if (!ref.HasValue(live)) ref.SetValue(live, value);
            }
        }
        auto &signalBindings = std::get<std::optional<SignalBindings>>(flatEntity);
        if (signalBindings) {
            for (auto &[signalName, binding] : signalBindings.value().bindings) {
                if (!binding) continue;
                SignalRef ref(liveId, signalName);
                if (!ref.HasBinding(live)) ref.SetBinding(live, binding);
            }
        }
    }
} // namespace sp::scene
