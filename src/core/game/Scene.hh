/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/Logging.hh"
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

        ecs::Name GenerateEntityName(const ecs::EntityScope &prefix);
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
            const std::string &path,
            SceneType type,
            ScenePriority priority,
            const ecs::SceneProperties &properties,
            std::shared_ptr<const Asset> asset = nullptr);

        // Should only be called from SceneManager thread
        ecs::Entity NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            ecs::Name name = ecs::Name());

        // Should only be called from SceneManager thread
        ecs::Entity NewRootEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            ecs::Name name,
            const ecs::EntityScope &scope);

        // Should only be called from SceneManager thread
        ecs::Entity NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            ecs::Entity prefabRoot,
            size_t prefabScriptId,
            std::string relativeName,
            const ecs::EntityScope &scope);

        // Should only be called from SceneManager thread
        void RemovePrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock, ecs::Entity ent);

        using SceneApplyCallback = std::function<void(const ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> &,
            const ecs::Lock<ecs::AddRemove> &)>;

        // Should only be called from SceneManager thread
        void ApplyScene(bool resetLive = false, SceneApplyCallback callback = nullptr);

        // Should only be called from SceneManager thread
        void RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live);

        // Should only be called from SceneManager thread
        void UpdateSceneProperties();
    };
} // namespace sp
