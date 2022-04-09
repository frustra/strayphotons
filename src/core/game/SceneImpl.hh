#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/SceneType.hh"

#include <bitset>

namespace sp::scene {
    template<typename T>
    inline void ApplyComponent(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if constexpr (!Tecs::is_global_component<T>()) { ecs::Component<T>().ApplyComponent(src, srcEnt, dst, dstEnt); }
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    inline void ApplyAllComponents(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::ReadAll> src,
        ecs::Entity srcEnt,
        Tecs::Lock<ECSType<AllComponentTypes...>, ecs::AddRemove> dst,
        ecs::Entity dstEnt) {
        (ApplyComponent<AllComponentTypes>(src, srcEnt, dst, dstEnt), ...);
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
        if constexpr (!Tecs::is_global_component<T>()) {
            if (ent.Has<T>(lock) && !hasComponents[ecs::ECS::template GetComponentIndex<T>()]) { ent.Unset<T>(lock); }
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
} // namespace sp::scene
