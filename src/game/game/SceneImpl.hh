#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"
#include "ecs/components/Transform.h"

#include <bitset>

namespace sp::scene {
    using namespace ecs;

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void BuildAndApplyEntity(Tecs::Lock<ECSType<AllComponentTypes...>, ReadAll> staging,
        Lock<AddRemove> live,
        Entity e,
        bool resetLive) {
        Assert(e.Has<SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        const SceneInfo &rootSceneInfo = e.Get<SceneInfo>(staging);
        Assert(rootSceneInfo.liveId.Has<SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        Assert(rootSceneInfo.rootStagingId == e, "Expected supplied entity to be the root stagingId");

        // Build a flattened set of components before applying to the real live id
        std::tuple<std::optional<AllComponentTypes>...> stagingComponents;
        auto stagingId = e;
        while (stagingId.Has<SceneInfo>(staging)) {
            auto &stagingInfo = stagingId.Get<SceneInfo>(staging);

            ( // For each component:
                [&] {
                    using T = AllComponentTypes;

                    if constexpr (std::is_same_v<T, Name>) {
                        // Ignore, this is handled below
                    } else if constexpr (std::is_same_v<T, SceneInfo>) {
                        // Ignore, this is handled by the scene
                    } else if constexpr (std::is_same_v<T, TransformSnapshot>) {
                        // Ignore, this is handled by TransformTree
                    } else if constexpr (!Tecs::is_global_component<T>()) {
                        if (!stagingId.Has<T>(staging)) return;

                        auto &component = std::get<std::optional<T>>(stagingComponents);
                        if (!component) {
                            auto comp = LookupComponent(typeid(T));
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
                        } else if constexpr (std::is_same_v<T, Animation>) {
                            auto animation = stagingId.Get<Animation>(staging);

                            // Apply scene root transform
                            if (stagingId.Has<TransformTree>(staging)) {
                                auto &transform = stagingId.Get<TransformTree>(staging);
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
                            }

                            LookupComponent<Animation>().ApplyComponent(component.value(), animation, false);
                        } else {
                            auto &srcComp = stagingId.Get<T>(staging);
                            LookupComponent<T>().ApplyComponent(component.value(), srcComp, false);
                        }
                    }
                }(),
                ...);

            stagingId = stagingInfo.nextStagingId;
        }

        // Apply flattened staging components to the live id, and remove any components that are no longer in staging
        ( // For each component:
            [&] {
                using T = AllComponentTypes;
                if constexpr (std::is_same_v<T, Name>) {
                    if (e.Has<Name>(staging)) {
                        rootSceneInfo.liveId.Set<Name>(live, e.Get<Name>(staging));
                    }
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    auto &component = std::get<std::optional<T>>(stagingComponents);
                    if (component) {
                        if (resetLive) {
                            rootSceneInfo.liveId.Unset<T>(live);
                        }

                        auto &dstComp = rootSceneInfo.liveId.Get<T>(live);
                        LookupComponent<T>().ApplyComponent(dstComp, *component, true);
                    } else if (rootSceneInfo.liveId.Has<T>(live)) {
                        if constexpr (std::is_same_v<T, TransformSnapshot>) {
                            auto &transformOpt = std::get<std::optional<TransformTree>>(stagingComponents);
                            if (!transformOpt) {
                                rootSceneInfo.liveId.Unset<TransformSnapshot>(live);
                            }
                        } else if constexpr (std::is_same_v<T, SceneInfo>) {
                            // Ignore, this should always be set
                        } else {
                            rootSceneInfo.liveId.Unset<T>(live);
                        }
                    }
                }
            }(),
            ...);
    }
} // namespace sp::scene
