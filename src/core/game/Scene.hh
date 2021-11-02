#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"

#include <robin_hood.h>

namespace sp {
    class Asset;

    class Scene : public NonCopyable {
    public:
        enum class Priority : int {
            System,
            Scene,
            Player,
            Bindings,
            Override,
        };

    public:
        Scene() {}
        Scene(const string &name, Priority priority) : sceneName(name), priority(priority) {}
        Scene(const string &name, shared_ptr<const Asset> asset) : sceneName(name), asset(asset) {}
        ~Scene() {}

        const string sceneName;
        robin_hood::unordered_flat_map<std::string, Tecs::Entity> namedEntities;

        vector<string> autoExecList, unloadExecList;

        void ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging, ecs::Lock<ecs::AddRemove> live);
        void RemoveScene(ecs::Lock<ecs::Read<ecs::Name, ecs::SceneInfo>> staging, ecs::Lock<ecs::AddRemove> live);

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
        Priority priority = Priority::Scene;
        shared_ptr<const Asset> asset;

        friend class SceneManager;
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
