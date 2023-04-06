#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/SceneRef.hh"

namespace sp {
    class Asset;
    struct SceneMetadata;

    class Scene {
    public:
        ~Scene();

        ecs::Entity GetStagingEntity(const ecs::Name &entityName) const;

        std::shared_ptr<const SceneMetadata> data;

    private:
        Scene(SceneMetadata &&metadata, std::shared_ptr<const Asset> asset = nullptr);
        friend class SceneManager;

        ecs::Name GenerateEntityName(const ecs::Name &prefix);
        bool ValidateEntityName(const ecs::Name &name) const;

        std::shared_ptr<const Asset> asset;
        bool active = false;
        size_t unnamedEntityCount = 0;

        std::unordered_map<ecs::Name, ecs::Entity> namedEntities;
        std::vector<ecs::EntityRef> references;

    public:
        // ==== Below functions are defined in game module: game/game/Scene.cc

        static std::shared_ptr<Scene> New(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::string &name,
            SceneType type,
            ScenePriority priority,
            std::shared_ptr<const Asset> asset = nullptr);

        // Should only be called from SceneManager thread
        ecs::Entity NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            ecs::Name name = ecs::Name());

        // Should only be called from SceneManager thread
        ecs::Entity NewRootEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            std::string relativeName = "");

        // Should only be called from SceneManager thread
        ecs::Entity NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            ecs::Entity prefabRoot,
            size_t prefabScriptId,
            std::string relativeName = "",
            ecs::EntityScope scope = ecs::Name());

        // Should only be called from SceneManager thread
        void RemovePrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock, ecs::Entity ent);

        // Should only be called from SceneManager thread
        void ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
            ecs::Lock<ecs::AddRemove> live,
            bool resetLive = false);

        // Should only be called from SceneManager thread
        void RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live);

        // Should only be called from SceneManager thread
        void UpdateSceneProperties();
    };
} // namespace sp
