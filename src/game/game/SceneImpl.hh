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
    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    auto BuildEntity(const Tecs::Lock<ECSType<AllComponentTypes...>, ReadAll> &staging, const Entity &e) {
        ZoneScoped;
        std::tuple<std::optional<AllComponentTypes>...> flatEntity;
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
    void BuildAndApplyEntity(const Tecs::Lock<ECSType<AllComponentTypes...>, ReadAll> &staging,
        const Lock<AddRemove> &live,
        const Entity &e,
        bool resetLive) {
        ZoneScoped;
        Assert(e.Has<SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        const SceneInfo &rootSceneInfo = e.Get<SceneInfo>(staging);
        Assert(rootSceneInfo.liveId.Has<SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        Assert(rootSceneInfo.rootStagingId == e, "Expected supplied entity to be the root stagingId");

        // Build a flattened set of components before applying to the real live id
        auto flatEntity = BuildEntity(staging, e);

        // Apply flattened staging components to the live id, and remove any components that are no longer in staging
        ( // For each component:
            [&] {
                using T = AllComponentTypes;
                if constexpr (std::is_same_v<T, Name>) {
                    if (e.Has<Name>(staging)) {
                        rootSceneInfo.liveId.Set<Name>(live, e.Get<Name>(staging));
                    }
                } else if constexpr (std::is_same_v<T, SceneInfo>) {
                    // Ignore, this should always be set

                } else if constexpr (std::is_same_v<T, SignalOutput>) {
                    // Skip, this is handled bellow
                } else if constexpr (std::is_same_v<T, SignalBindings>) {
                    // Skip, this is handled bellow
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    auto &component = std::get<std::optional<T>>(flatEntity);
                    if (component) {
                        if (resetLive) {
                            rootSceneInfo.liveId.Unset<T>(live);
                        }

                        auto &dstComp = rootSceneInfo.liveId.Get<T>(live);
                        LookupComponent<T>().ApplyComponent(dstComp, *component, true);
                    } else if (rootSceneInfo.liveId.Has<T>(live)) {
                        if constexpr (std::is_same_v<T, TransformSnapshot>) {
                            auto &transformOpt = std::get<std::optional<TransformTree>>(flatEntity);
                            if (!transformOpt) {
                                rootSceneInfo.liveId.Unset<TransformSnapshot>(live);
                            }
                        } else {
                            rootSceneInfo.liveId.Unset<T>(live);
                        }
                    }
                }
            }(),
            ...);

        if (resetLive) {
            GetSignalManager().ClearEntity(live, rootSceneInfo.liveId);
        }

        auto &signalOutput = std::get<std::optional<SignalOutput>>(flatEntity);
        if (signalOutput) {
            for (auto &[signalName, value] : signalOutput.value().signals) {
                if (!std::isfinite(value)) continue;
                SignalRef(rootSceneInfo.liveId, signalName).SetValue(live, value);
            }
        }
        auto &signalBindings = std::get<std::optional<SignalBindings>>(flatEntity);
        if (signalBindings) {
            for (auto &[signalName, binding] : signalBindings.value().bindings) {
                if (!binding) continue;
                SignalRef(rootSceneInfo.liveId, signalName).SetBinding(live, binding);
            }
        }
    }
} // namespace sp::scene
