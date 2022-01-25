#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/SceneInfo.hh"
#include "ecs/components/Transform.h"
#include "game/SceneType.hh"

#include <robin_hood.h>

namespace sp {
    class Asset;

    class Scene : public NonCopyable {
    public:
        Scene() : type(SceneType::Count) {}
        Scene(const string &name, SceneType type) : name(name), type(type) {}
        Scene(const string &name, SceneType type, shared_ptr<const Asset> asset)
            : name(name), type(type), asset(asset) {}
        ~Scene();

        const std::string name;
        const SceneType type;
        robin_hood::unordered_flat_map<std::string, Tecs::Entity> namedEntities;

        void ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging, ecs::Lock<ecs::AddRemove> live);
        void RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live);

        template<typename T>
        static void CopyComponent(ecs::Lock<ecs::ReadAll> src,
            Tecs::Entity srcEnt,
            ecs::Lock<ecs::AddRemove> dst,
            Tecs::Entity dstEnt) {
            if constexpr (!Tecs::is_global_component<T>()) {
                if (srcEnt.Has<T>(src) && !dstEnt.Has<T>(dst)) dstEnt.Set<T>(dst, srcEnt.Get<T>(src));
            }
        }

        template<typename... AllComponentTypes, template<typename...> typename ECSType>
        static void CopyAllComponents(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::ReadAll> src,
            Tecs::Entity srcEnt,
            Tecs::Lock<ECSType<AllComponentTypes...>, ecs::AddRemove> dst,
            Tecs::Entity dstEnt) {
            (CopyComponent<AllComponentTypes>(src, srcEnt, dst, dstEnt), ...);
        }

    private:
        std::shared_ptr<const Asset> asset;

        ecs::ECS *liveWorld = nullptr;
        ecs::ECS *stagingWorld = nullptr;

        friend class SceneManager;
    };

    template<>
    inline void Scene::CopyComponent<ecs::Transform>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::Transform>(src) && !dstEnt.Has<ecs::Transform>(dst)) {
            auto &srcTransform = srcEnt.Get<ecs::Transform>(src);
            auto &dstTransform = dstEnt.Get<ecs::Transform>(dst);

            // Map transform parent from staging id to live id
            auto parent = srcTransform.GetParent();
            if (parent && parent.Has<ecs::SceneInfo>(src)) {
                auto &sceneInfo = parent.Get<ecs::SceneInfo>(src);
                dstTransform.SetParent(sceneInfo.liveId);
            } else {
                dstTransform.SetParent(Tecs::Entity());
            }
            dstTransform.SetPosition(srcTransform.GetPosition());
            dstTransform.SetRotation(srcTransform.GetRotation());
            dstTransform.SetScale(srcTransform.GetScale());
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::Light>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::Light>(src) && !dstEnt.Has<ecs::Light>(dst)) {
            auto &light = dstEnt.Set<ecs::Light>(dst, srcEnt.Get<ecs::Light>(src));

            // Map light bulb from staging id to live id
            if (light.bulb && light.bulb.Has<ecs::SceneInfo>(src)) {
                auto &sceneInfo = light.bulb.Get<ecs::SceneInfo>(src);
                light.bulb = sceneInfo.liveId;
            }
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::Physics>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::Physics>(src) && !dstEnt.Has<ecs::Physics>(dst)) {
            auto &physics = dstEnt.Set<ecs::Physics>(dst, srcEnt.Get<ecs::Physics>(src));

            // Map physics joints from staging id to live id
            if (physics.jointTarget && physics.jointTarget.Has<ecs::SceneInfo>(src)) {
                auto &sceneInfo = physics.jointTarget.Get<ecs::SceneInfo>(src);
                physics.jointTarget = sceneInfo.liveId;
            }

            // Map physics constraint from staging id to live id
            if (physics.constraint && physics.constraint.Has<ecs::SceneInfo>(src)) {
                auto &sceneInfo = physics.constraint.Get<ecs::SceneInfo>(src);
                physics.constraint = sceneInfo.liveId;
            }
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::Script>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::Script>(src)) {
            auto &srcScript = srcEnt.Get<ecs::Script>(src);
            auto &dstScript = dstEnt.Get<ecs::Script>(dst);
            dstScript.CopyCallbacks(srcScript);
            dstScript.CopyParams(srcScript);
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::SceneConnection>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::SceneConnection>(src)) {
            auto &srcConnection = srcEnt.Get<ecs::SceneConnection>(src);
            auto &dstConnection = dstEnt.Get<ecs::SceneConnection>(dst);
            for (auto &scene : srcConnection.scenes) {
                if (!dstConnection.HasScene(scene)) dstConnection.scenes.emplace_back(scene);
            }
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::SignalBindings>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::SignalBindings>(src)) {
            auto &srcBindings = srcEnt.Get<ecs::SignalBindings>(src);
            auto &dstBindings = dstEnt.Get<ecs::SignalBindings>(dst);
            dstBindings.CopyBindings(srcBindings);
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::SignalOutput>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::SignalOutput>(src)) {
            auto &srcOutput = srcEnt.Get<ecs::SignalOutput>(src);
            auto &dstOutput = dstEnt.Get<ecs::SignalOutput>(dst);
            for (auto &signal : srcOutput.GetSignals()) {
                if (!dstOutput.HasSignal(signal.first)) dstOutput.SetSignal(signal.first, signal.second);
            }
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::TriggerArea>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::TriggerArea>(src)) {
            auto &srcArea = srcEnt.Get<ecs::TriggerArea>(src);
            auto &dstArea = dstEnt.Get<ecs::TriggerArea>(dst);
            for (size_t i = 0; i < srcArea.triggers.size(); i++) {
                auto &srcTriggers = srcArea.triggers.at(i);
                auto &dstTriggers = dstArea.triggers.at(i);
                dstTriggers.insert(dstTriggers.end(), srcTriggers.begin(), srcTriggers.end());
            }
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::EventBindings>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::EventBindings>(src)) {
            auto &srcBindings = srcEnt.Get<ecs::EventBindings>(src);
            auto &dstBindings = dstEnt.Get<ecs::EventBindings>(dst);
            dstBindings.CopyBindings(srcBindings);
        }
    }

    template<>
    inline void Scene::CopyComponent<ecs::EventInput>(ecs::Lock<ecs::ReadAll> src,
        Tecs::Entity srcEnt,
        ecs::Lock<ecs::AddRemove> dst,
        Tecs::Entity dstEnt) {
        if (srcEnt.Has<ecs::EventInput>(src)) {
            auto &srcInput = srcEnt.Get<ecs::EventInput>(src);
            auto &dstInput = dstEnt.Get<ecs::EventInput>(dst);
            for (auto &input : srcInput.events) {
                if (!dstInput.IsRegistered(input.first)) dstInput.Register(input.first);
            }
        }
    }
} // namespace sp
