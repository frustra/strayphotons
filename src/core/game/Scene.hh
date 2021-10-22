#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"

#include <robin_hood.h>

namespace sp {
    class Asset;

    class Scene : public NonCopyable {
    public:
        Scene() {}
        Scene(const string &name, shared_ptr<const Asset> asset) : name(name), asset(asset) {}
        ~Scene() {}

        const string name;
        vector<Tecs::Entity> entities;
        robin_hood::unordered_flat_map<std::string, Tecs::Entity> namedEntities;

        vector<string> autoExecList, unloadExecList;

        void CopyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> src, ecs::Lock<ecs::AddRemove> dst);

        template<typename T>
        void CopyComponent(ecs::Lock<ecs::ReadAll> src,
            Tecs::Entity srcEnt,
            ecs::Lock<ecs::AddRemove> dst,
            Tecs::Entity dstEnt) {
            if constexpr (!Tecs::is_global_component<T>()) {
                if (srcEnt.Has<T>(src)) dstEnt.Set<T>(dst, srcEnt.Get<T>(src));
            }
        }

        template<typename... AllComponentTypes, template<typename...> typename ECSType>
        void CopyAllComponents(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::ReadAll> src,
            Tecs::Entity srcEnt,
            Tecs::Lock<ECSType<AllComponentTypes...>, ecs::AddRemove> dst,
            Tecs::Entity dstEnt) {
            (CopyComponent<AllComponentTypes>(src, srcEnt, dst, dstEnt), ...);
        }

    private:
        shared_ptr<const Asset> asset;
    };

    template<>
    inline void Scene::CopyComponent<ecs::Transform>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::Transform>(src)) {
            auto &transform = dstEnt.Set<ecs::Transform>(dst, srcEnt.Get<ecs::Transform>(src));

            // Map transform parent from staging id to live id
            auto parent = transform.GetParent();
            if (parent && parent.Has<ecs::SceneInfo>(src)) {
                auto &sceneInfo = parent.Get<ecs::SceneInfo>(src);
                transform.SetParent(sceneInfo.liveId);
            }
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::Light>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::Light>(src)) {
            auto &light = dstEnt.Set<ecs::Light>(dst, srcEnt.Get<ecs::Light>(src));

            // Map light bulb from staging id to live id
            if (light.bulb && light.bulb.Has<ecs::SceneInfo>(src)) {
                auto &sceneInfo = light.bulb.Get<ecs::SceneInfo>(src);
                light.bulb = sceneInfo.liveId;
            }
        }
    }
} // namespace sp
