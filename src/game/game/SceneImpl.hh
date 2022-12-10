#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"
#include "ecs/components/Transform.h"

#include <bitset>

namespace sp::scene {
    template<typename T>
    inline void ApplyComponent(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if constexpr (std::is_same<T, ecs::Name>()) {
            if (srcEnt.Has<T>(src) && !dstEnt.Has<T>(dst)) dstEnt.Set<T>(dst, srcEnt.Get<T>(src));
        } else if constexpr (std::is_same<T, ecs::SceneInfo>()) {
            // Ignore, this is handled by the scene
        } else if constexpr (std::is_same<T, ecs::TransformSnapshot>()) {
            // Ignore, this is handled by TransformTree
        } else if constexpr (!Tecs::is_global_component<T>()) {
            if (!srcEnt.Has<T>(src)) return;

            auto &srcComp = srcEnt.Get<T>(src);
            T *dstComp;
            if (!dstEnt.Has<T>(dst)) {
                dstComp = &dstEnt.Set<T>(dst, ecs::LookupComponent<T>().GetDefault(IsLive(dst)));
            } else {
                dstComp = &dstEnt.Get<T>(dst);
            }

            // Apply scene root transforms
            // TODO: Only do this when applying to the live ECS
            if constexpr (std::is_same<T, ecs::TransformTree>()) {
                ecs::TransformTree transform(srcComp.pose.Get(), srcComp.parent);
                if (!srcComp.parent) {
                    auto &rootTransform = ecs::SceneInfo::GetRootTransform(src, srcEnt);
                    if (rootTransform != ecs::Transform()) {
                        transform.pose = rootTransform * transform.pose;
                    }
                }
                ecs::LookupComponent<T>().ApplyComponent(*dstComp, transform, IsLive(dst));
            } else if constexpr (std::is_same<T, ecs::Animation>()) {
                if (srcEnt.Has<ecs::TransformTree>(src)) {
                    auto &srcTransform = srcEnt.Get<ecs::TransformTree>(src);
                    auto animation = srcComp;
                    if (!srcTransform.parent) {
                        auto &rootTransform = ecs::SceneInfo::GetRootTransform(src, srcEnt);
                        if (rootTransform != ecs::Transform()) {
                            for (auto &state : animation.states) {
                                state.pos = rootTransform * glm::vec4(state.pos, 1);
                            }
                        }
                    }
                    ecs::LookupComponent<T>().ApplyComponent(*dstComp, animation, IsLive(dst));
                } else {
                    ecs::LookupComponent<T>().ApplyComponent(*dstComp, srcComp, IsLive(dst));
                }
            } else {
                ecs::LookupComponent<T>().ApplyComponent(*dstComp, srcComp, IsLive(dst));
            }
        }
    }

    template<typename T>
    inline void RemoveComponent(ecs::Lock<ecs::AddRemove> lock, Tecs::Entity ent) {
        if constexpr (std::is_same<T, ecs::Name>()) {
            // Ignore, scene entities should always have a Name
        } else if constexpr (std::is_same<T, ecs::SceneInfo>()) {
            // Ignore, scene entities should always have SceneInfo
        } else if constexpr (!Tecs::is_global_component<T>()) {
            if (ent.Has<T>(lock)) ent.Unset<T>(lock);
        }
    }

    template<typename T, typename BitsetType>
    inline void MarkHasComponent(ecs::Lock<> lock, ecs::Entity ent, BitsetType &hasComponents) {
        if constexpr (!Tecs::is_global_component<T>()) {
            if (ent.Has<T>(lock)) hasComponents[ecs::ECS::template GetComponentIndex<T>()] = true;
        }
    }

    template<typename T, typename BitsetType>
    inline void RemoveDanglingComponent(ecs::Lock<ecs::AddRemove> lock,
        ecs::Entity ent,
        const BitsetType &hasComponents) {
        if constexpr (std::is_same<T, ecs::TransformSnapshot>()) {
            if (ent.Has<ecs::TransformSnapshot>(lock) &&
                !hasComponents[ecs::ECS::GetComponentIndex<ecs::TransformTree>()]) {
                ent.Unset<ecs::TransformSnapshot>(lock);
            }
        } else if constexpr (std::is_same<T, ecs::SceneInfo>()) {
            // Ignore, this should always be set
        } else if constexpr (!Tecs::is_global_component<T>()) {
            if (ent.Has<T>(lock) && !hasComponents[ecs::ECS::template GetComponentIndex<T>()]) {
                ent.Unset<T>(lock);
            }
        }
    }

    // Remove components from a live entity that no longer exist in any staging entity
    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    inline void RemoveDanglingComponents(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::ReadAll> staging,
        Tecs::Lock<ECSType<AllComponentTypes...>, ecs::AddRemove> live,
        ecs::Entity liveId) {
        Assert(liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        auto &liveSceneInfo = liveId.Get<const ecs::SceneInfo>(live);

        std::bitset<ECSType<AllComponentTypes...>::GetComponentCount()> hasComponents;
        auto stagingId = liveSceneInfo.stagingId;
        while (stagingId.template Has<ecs::SceneInfo>(staging)) {
            (MarkHasComponent<AllComponentTypes>(staging, stagingId, hasComponents), ...);

            auto &stagingInfo = stagingId.template Get<ecs::SceneInfo>(staging);
            stagingId = stagingInfo.nextStagingId;
        }
        (RemoveDanglingComponent<AllComponentTypes>(live, liveId, hasComponents), ...);
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void BuildAndApplyEntity(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::ReadAll> staging,
        ecs::Lock<ecs::AddRemove> live,
        ecs::Entity e,
        bool resetLive = false) {
        Assert(e.Has<ecs::SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        auto &rootSceneInfo = e.Get<ecs::SceneInfo>(staging);
        Assert(rootSceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        Assert(rootSceneInfo.stagingId == e, "Expected supplied entity to be the root stagingId");
        auto &liveSceneInfo = rootSceneInfo.liveId.Get<const ecs::SceneInfo>(live);

        std::vector<ecs::Entity> stagingIds;
        auto stagingId = liveSceneInfo.stagingId;
        while (stagingId.Has<ecs::SceneInfo>(staging)) {
            stagingIds.emplace_back(stagingId);

            auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(staging);
            stagingId = stagingInfo.nextStagingId;
        }

        if (resetLive) {
            (RemoveComponent<AllComponentTypes>(live, rootSceneInfo.liveId), ...);
        }

        // TODO: Build a temporary entity before applying to the real live id
        while (!stagingIds.empty()) {
            (ApplyComponent<AllComponentTypes>(staging, stagingIds.back(), live, rootSceneInfo.liveId), ...);
            stagingIds.pop_back();
        }
        scene::RemoveDanglingComponents(staging, live, rootSceneInfo.liveId);

        // if (liveSceneInfo.stagingId == e) {
        //     while (e.Has<ecs::SceneInfo>(staging)) {
        //         // Entity is the linked-list root, which can be applied directly.
        //         scene::ApplyAllComponents(ecs::Lock<ecs::ReadAll>(staging), e, live, rootSceneInfo.liveId);
        //         e = e.Get<ecs::SceneInfo>(staging).nextStagingId;
        //     }
        // } else {
        //     RebuildComponentsByPriority(staging, live, e);
        // }
    }
} // namespace sp::scene
