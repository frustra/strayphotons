#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/SceneType.hh"

#include <bitset>
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

        ecs::Entity NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            ecs::Name name = ecs::Name());
        ecs::Entity NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            ecs::Entity prefabRoot,
            ecs::Name name = ecs::Name());

        void ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging, ecs::Lock<ecs::AddRemove> live);
        void RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live);

        ecs::Entity GetEntity(const ecs::Name &entityName) const {
            auto it = namedEntities.find(entityName);
            if (it != namedEntities.end()) return it->second;
            return {};
        }

        ecs::Entity GetEntity(const std::string &fullName) const {
            ecs::Name entityName;
            if (entityName.Parse(fullName, this)) {
                auto it = namedEntities.find(entityName);
                if (it != namedEntities.end()) return it->second;
            } else {
                Errorf("Invalid entity name: %s", fullName);
            }
            return {};
        }

        template<typename T>
        static void ApplyComponent(ecs::Lock<ecs::ReadAll> src,
            Tecs::Entity srcEnt,
            ecs::Lock<ecs::AddRemove> dst,
            Tecs::Entity dstEnt) {
            if constexpr (!Tecs::is_global_component<T>()) {
                ecs::Component<T>().ApplyComponent(src, srcEnt, dst, dstEnt);
            }
        }

        template<typename... AllComponentTypes, template<typename...> typename ECSType>
        static void ApplyAllComponents(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::ReadAll> src,
            ecs::Entity srcEnt,
            Tecs::Lock<ECSType<AllComponentTypes...>, ecs::AddRemove> dst,
            ecs::Entity dstEnt) {
            (ApplyComponent<AllComponentTypes>(src, srcEnt, dst, dstEnt), ...);
        }

        template<typename T, typename BitsetType>
        static void MarkHasComponent(ecs::Lock<> lock, ecs::Entity ent, BitsetType &hasComponents) {
            if constexpr (!Tecs::is_global_component<T>()) {
                if (ent.Has<T>(lock)) hasComponents[ecs::ECS::template GetComponentIndex<T>()] = true;
            }
        }

        template<typename T, typename BitsetType>
        static void RemoveDanglingComponent(ecs::Lock<ecs::AddRemove> lock,
            ecs::Entity ent,
            const BitsetType &hasComponents) {
            if constexpr (!Tecs::is_global_component<T>()) {
                if (ent.Has<T>(lock) && !hasComponents[ecs::ECS::template GetComponentIndex<T>()]) {
                    ent.Unset<T>(lock);
                }
            }
        }

        // Remove components from a live entity that no longer exist in any staging entity
        template<typename... AllComponentTypes, template<typename...> typename ECSType>
        static void RemoveDanglingComponents(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::ReadAll> staging,
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

    private:
        std::shared_ptr<const Asset> asset;
        bool active = false;

        std::unordered_map<ecs::Name, ecs::Entity> namedEntities;

        friend class SceneManager;
    };
} // namespace sp
