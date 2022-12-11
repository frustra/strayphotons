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
        const SceneInfo &liveSceneInfo = rootSceneInfo.liveId.Get<const SceneInfo>(live);

        // Build a flattened set of components before applying to the real live id
        std::tuple<std::optional<AllComponentTypes>...> stagingComponents;
        auto stagingId = liveSceneInfo.rootStagingId;
        while (stagingId.Has<SceneInfo>(staging)) {
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
                        if (!component) component = LookupComponent<T>().GetStagingDefault();

                        auto &srcComp = stagingId.Get<T>(staging);
                        LookupComponent<T>().ApplyComponent(component.value(), srcComp, false);
                    }
                }(),
                ...);

            auto &stagingInfo = stagingId.Get<SceneInfo>(staging);
            stagingId = stagingInfo.nextStagingId;
        }

        // Apply scene root transforms
        auto &transformOpt = std::get<std::optional<TransformTree>>(stagingComponents);
        if (transformOpt) {
            auto &transform = transformOpt.value();
            if (!transform.parent) {
                auto scene = rootSceneInfo.scene.lock();
                auto &rootTransform = scene->GetRootTransform();
                if (rootTransform != Transform()) {
                    transform.pose = rootTransform * transform.pose.Get();

                    auto &animationOpt = std::get<std::optional<Animation>>(stagingComponents);
                    if (animationOpt) {
                        auto &animation = animationOpt.value();
                        for (auto &state : animation.states) {
                            state.pos = rootTransform * glm::vec4(state.pos, 1);
                        }
                    }
                }
            }
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
                            rootSceneInfo.liveId.Set<T>(live, *component);
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
