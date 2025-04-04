/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "game/Scene.hh"

#include "common/Common.hh"
#include "common/Tracing.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/ScriptManager.hh"
#include "game/SceneImpl.hh"
#include "game/SceneManager.hh"

namespace sp {
    std::shared_ptr<Scene> Scene::New(ecs::Lock<ecs::AddRemove> stagingLock,
        const std::string &name,
        const std::string &path,
        SceneType type,
        ScenePriority priority,
        const ecs::SceneProperties &properties,
        std::shared_ptr<const Asset> asset) {
        Assert(IsStaging(stagingLock), "Scene::New must be called with a Staging lock");

        auto sceneId = stagingLock.NewEntity();
        ecs::Name sceneName("scene", name);
        sceneId.Set<ecs::Name>(stagingLock, sceneName);
        sceneId.Set<ecs::SceneProperties>(stagingLock, properties);
        return std::make_shared<Scene>(Scene{SceneMetadata{name, path, type, priority, sceneId}, asset});
    }

    ecs::Entity Scene::NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        const std::shared_ptr<Scene> &scene,
        ecs::Name entityName) {
        Assertf(ecs::IsStaging(stagingLock), "Scene::NewSystemEntity must be called with a staging lock");
        Assertf(scene, "Scene::NewSystemEntity called with null scene: %s", entityName.String());
        Assertf(scene->data, "Scene::NewSystemEntity called with null scene data: %s", entityName.String());
        Assertf(scene->data->priority == ScenePriority::System,
            "Scene::NewSystemEntity called on non-system scene: %s",
            scene->data->name);

        if (entityName) {
            if (!ValidateEntityName(entityName)) {
                Errorf("Invalid system entity name: %s", entityName.String());
                return ecs::Entity();
            }

            if (GetStagingEntity(entityName)) {
                Errorf("Duplicate system entity name: %s", entityName.String());
                return ecs::Entity();
            }
        } else {
            entityName = GenerateEntityName(entityName);
        }

        if (!entityName) {
            Errorf("Invalid system entity name: %s", entityName.String());
            return ecs::Entity();
        } else if (namedEntities.count(entityName) > 0) {
            Errorf("Duplicate system entity name: %s", entityName.String());
            return ecs::Entity();
        }

        auto entity = stagingLock.NewEntity();
        entity.Set<ecs::SceneInfo>(stagingLock, entity, scene, ecs::EntityScope(scene->data->name, ""));
        entity.Set<ecs::Name>(stagingLock, entityName);
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    ecs::Entity Scene::NewRootEntity(ecs::Lock<ecs::AddRemove> lock,
        const std::shared_ptr<Scene> &scene,
        ecs::Name entityName,
        const ecs::EntityScope &scope) {
        Assertf(scene, "Scene::NewRootEntity called with null scene: %s", entityName.String());
        Assertf(scene->data, "Scene::NewRootEntity called with null scene data: %s", entityName.String());

        if (entityName) {
            if (scene->data->priority != ScenePriority::SaveGame && !ValidateEntityName(entityName)) {
                Errorf("Invalid root entity name: %s", entityName.String());
                return ecs::Entity();
            }

            if (GetStagingEntity(entityName)) {
                Errorf("Duplicate root entity name: %s", entityName.String());
                return ecs::Entity();
            }
        } else {
            entityName = GenerateEntityName(ecs::Name(scene->data->name, ""));
        }

        if (!entityName) {
            Errorf("Invalid root entity name in scene %s: %s", scene->data->name, entityName.String());
            return ecs::Entity();
        } else if (namedEntities.count(entityName) > 0) {
            Errorf("Duplicate root entity name: %s", entityName.String());
            return ecs::Entity();
        }

        auto entity = lock.NewEntity();
        entity.Set<ecs::SceneInfo>(lock, entity, scene, scope);
        entity.Set<ecs::Name>(lock, entityName);
        if (ecs::IsLive(lock)) entity.Set<ecs::SceneProperties>(lock, scene->data->GetProperties(lock));
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    ecs::Entity Scene::NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        ecs::Entity prefabRoot,
        size_t prefabScriptId,
        std::string relativeName,
        const ecs::EntityScope &scope) {
        Assertf(ecs::IsStaging(stagingLock), "Scene::NewPrefabEntity must be called with a staging lock");
        Assertf(prefabRoot.Has<ecs::SceneInfo>(stagingLock),
            "Prefab root %s does not have SceneInfo",
            ecs::ToString(stagingLock, prefabRoot));
        Assertf(prefabRoot.Has<ecs::Scripts>(stagingLock),
            "Prefab root %s does not have Scripts component",
            ecs::ToString(stagingLock, prefabRoot));
        auto &prefabScripts = prefabRoot.Get<const ecs::Scripts>(stagingLock);
        Assertf(prefabScripts.FindScript(prefabScriptId) != nullptr,
            "Scene::NewPrefabEntity provided prefabScriptId not found in Scripts");

        ecs::Entity existing;
        ecs::Name entityName;
        if (!relativeName.empty()) {
            entityName = ecs::Name(relativeName, scope);
            if (relativeName != "scoperoot" && !ValidateEntityName(entityName)) {
                Errorf("Invalid prefab entity name: %s (scope: %s)", relativeName, scope.String());
                return ecs::Entity();
            }

            existing = GetStagingEntity(entityName);
        }

        if (!entityName) entityName = GenerateEntityName(scope);
        if (!entityName) {
            Errorf("Invalid root entity name: %s", entityName.String());
            return ecs::Entity();
        } else if (!existing && namedEntities.count(entityName) > 0) {
            Errorf("Duplicate generated prefab entity name: %s", entityName.String());
            return ecs::Entity();
        }

        auto entity = stagingLock.NewEntity();
        auto &rootSceneInfo = prefabRoot.Get<const ecs::SceneInfo>(stagingLock);
        entity.Set<ecs::Name>(stagingLock, entityName);
        auto &newSceneInfo = entity.Set<ecs::SceneInfo>(stagingLock,
            entity,
            prefabRoot,
            prefabScriptId,
            rootSceneInfo,
            scope);
        if (existing) {
            Assertf(existing.Has<ecs::SceneInfo>(stagingLock),
                "Expected existing staging entity to have SceneInfo: %s",
                entityName.String());

            auto &existingSceneInfo = existing.Get<ecs::SceneInfo>(stagingLock);
            newSceneInfo.SetLiveId(stagingLock, existingSceneInfo.liveId);
            existingSceneInfo.InsertWithPriority(stagingLock, newSceneInfo);
        } else {
            namedEntities.emplace(entityName, entity);
        }
        references.emplace_back(entityName, entity);

        return entity;
    }

    void Scene::RemovePrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock, ecs::Entity ent) {
        Assertf(ecs::IsStaging(stagingLock), "Scene::RemovePrefabEntity must be called with a staging lock");
        Assertf(ecs::IsStaging(ent), "Scene::RemovePrefabEntity must be called with a staging entity");
        if (!ent.Has<ecs::SceneInfo>(stagingLock)) return;
        auto &stagingInfo = ent.Get<ecs::SceneInfo>(stagingLock);
        Assertf(stagingInfo.prefabStagingId, "Scene::RemovePrefabEntity must be called with a prefab entity");

        if (!stagingInfo.rootStagingId.Has<ecs::SceneInfo>(stagingLock)) {
            ent.Destroy(stagingLock);
            return;
        }

        auto &rootSceneInfo = stagingInfo.rootStagingId.Get<ecs::SceneInfo>(stagingLock);
        if (!rootSceneInfo.Remove(stagingLock, ent)) {
            if (ent.Has<ecs::Name>(stagingLock)) {
                auto &name = ent.Get<ecs::Name>(stagingLock);
                namedEntities.erase(name);
                sp::erase_if(references, [&](auto &&ref) {
                    return ref.Name() == name;
                });
            }
        }
        ent.Destroy(stagingLock);
    }

    void Scene::ApplyScene(bool resetLive, SceneApplyCallback callback) {
        ZoneScoped;
        ZoneStr(data->name);
        Debugf("Applying scene: %s", data->name);
        Assertf(data->sceneEntity,
            "Scene::ApplyScene %s missing scene entity: %s",
            data->name,
            data->sceneEntity.Name().String());

        auto staging = ecs::StartStagingTransaction<ecs::ReadAll, ecs::Write<ecs::SceneInfo>>();
        auto stagingSceneId = data->sceneEntity.Get(staging);
        Assertf(stagingSceneId.Exists(staging),
            "Scene::ApplyScene %s missing staging scene entity: %s",
            data->name,
            data->sceneEntity.Name().String());

        // Build a flattened list of entities to apply from the staging ECS
        std::vector<std::pair<ecs::Entity, ecs::FlatEntity>> entities;
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene != *this) continue;
            if (sceneInfo.rootStagingId != e) {
                // Skip entities that aren't the root staging id
                continue;
            } else if (!e.Has<ecs::Name>(staging)) {
                Errorf("Scene contains unnamed entity: %s %s", data->name, ecs::ToString(staging, e));
                continue;
            }

            entities.emplace_back(e, scene_util::BuildEntity(ecs::Lock<ecs::ReadAll>(staging), e));
        }

        auto live = ecs::StartTransaction<ecs::AddRemove>();

        auto liveSceneId = data->sceneEntity.Get(live);
        if (!liveSceneId.Exists(live)) {
            liveSceneId = live.NewEntity();
            liveSceneId.Set<ecs::Name>(live, data->sceneEntity.Name());
            ecs::GetEntityRefs().Set(data->sceneEntity.Name(), liveSceneId);
        }
        auto &properties = liveSceneId.Set<ecs::SceneProperties>(live,
            ecs::SceneProperties::Get(staging, stagingSceneId));
        properties.fixedGravity = properties.rootTransform * glm::vec4(properties.fixedGravity, 0.0f);
        properties.gravityTransform = properties.rootTransform * properties.gravityTransform;

        for (auto e : live.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(live)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(live);
            if (sceneInfo.scene != *this) continue;
            Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");

            if (!sceneInfo.rootStagingId.Has<ecs::SceneInfo>(staging)) e.Destroy(live);
        }
        for (auto &[e, flatEntity] : entities) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);

            if (sceneInfo.liveId.Exists(live)) {
                // Entity has already been added, just rebuild it.
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                sceneInfo.SetLiveId(staging, sceneInfo.liveId);
                liveSceneInfo = sceneInfo.rootStagingId.Get<ecs::SceneInfo>(staging);

                scene_util::ApplyFlatEntity(live, sceneInfo.liveId, flatEntity, resetLive);
                continue;
            }

            auto &entityName = e.Get<const ecs::Name>(staging);
            // Find matching named entity in live scene
            sceneInfo.liveId = ecs::EntityRef(entityName).Get(live);
            if (sceneInfo.liveId.Exists(live)) {
                // Entity overlaps with another scene
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                liveSceneInfo.InsertWithPriority(staging, sceneInfo);
                sceneInfo.SetLiveId(staging, sceneInfo.liveId);
                liveSceneInfo = sceneInfo.rootStagingId.Get<ecs::SceneInfo>(staging);

                // Rebuild the flat entity since the scene hierarchy has changed
                flatEntity = scene_util::BuildEntity(ecs::Lock<ecs::ReadAll>(staging), liveSceneInfo.rootStagingId);
                scene_util::ApplyFlatEntity(live, sceneInfo.liveId, flatEntity, resetLive);
            } else {
                // No entity exists in the live scene
                sceneInfo.liveId = live.NewEntity();
                sceneInfo.liveId.Set<ecs::Name>(live, entityName);
                sceneInfo.SetLiveId(staging, sceneInfo.liveId);
                sceneInfo.liveId.Set<ecs::SceneInfo>(live, sceneInfo.rootStagingId.Get<ecs::SceneInfo>(staging));
                ecs::GetEntityRefs().Set(entityName, e);
                ecs::GetEntityRefs().Set(entityName, sceneInfo.liveId);

                scene_util::ApplyFlatEntity(live, sceneInfo.liveId, flatEntity, resetLive);
            }
        }
        {
            ZoneScopedN("AnimationUpdate");
            for (auto &e : live.EntitiesWith<ecs::Animation>()) {
                if (!e.Has<ecs::Animation, ecs::TransformTree>(live)) continue;

                ecs::Animation::UpdateTransform(live, e);
            }
        }
        ecs::GetScriptManager().RegisterEvents(live);
        {
            ZoneScopedN("TransformSnapshot");
            for (auto &e : live.EntitiesWith<ecs::TransformTree>()) {
                if (!e.Has<ecs::TransformTree>(live)) continue;

                auto transform = e.Get<ecs::TransformTree>(live).GetGlobalTransform(live);
                e.Set<ecs::TransformSnapshot>(live, transform);
            }
        }
        active = true;

        if (callback) callback(staging, live);
    }

    void Scene::RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live) {
        ZoneScoped;
        ZoneStr(data->name);
        Debugf("Removing scene: %s", data->name);
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(staging)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene != *this) continue;

            auto remainingId = sceneInfo.Remove(staging, e);
            if (sceneInfo.liveId) {
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                if (!remainingId.Has<ecs::SceneInfo>(staging)) {
                    // No more staging entities, remove the live id.
                    sceneInfo.liveId.Destroy(live);
                } else {
                    auto &remainingInfo = remainingId.Get<ecs::SceneInfo>(staging);
                    Assert(remainingInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                    remainingInfo.liveId.Set<ecs::SceneInfo>(live,
                        remainingInfo.rootStagingId.Get<ecs::SceneInfo>(staging));

                    auto flatEntity = scene_util::BuildEntity(ecs::Lock<ecs::ReadAll>(staging),
                        remainingInfo.rootStagingId);
                    scene_util::ApplyFlatEntity(live, remainingInfo.liveId, flatEntity, false);
                }
            }
            ecs::EntityRef ref = e;
            if (remainingId && ref.GetStaging() == e) {
                // Update the entity ref to point to the new staging entity root.
                ecs::GetEntityRefs().Set(ref.Name(), remainingId);
            }
            e.Destroy(staging);
        }
        for (auto &e : live.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(live)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(live);
            if (sceneInfo.scene != *this || sceneInfo.rootStagingId) continue;

            // Remove non-staging entities that were created by a script after scene load.
            e.Destroy(live);
        }

        auto liveSceneId = data->sceneEntity.Get(live);
        auto stagingSceneId = data->sceneEntity.Get(staging);
        if (liveSceneId.Exists(live)) liveSceneId.Destroy(live);
        if (stagingSceneId.Exists(staging)) stagingSceneId.Destroy(staging);

        live.Get<ecs::Signals>().UpdateMissingEntitySignals(live);

        active = false;
    }

    void Scene::UpdateSceneProperties() {
        ZoneScoped;
        ZoneStr(data->name);
        auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::TransformTree, ecs::SceneInfo>,
            ecs::Write<ecs::SceneProperties>>();
        auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneConnection, ecs::TransformSnapshot>>();

        ecs::Entity liveConnection, stagingConnection;
        for (auto &e : stagingLock.EntitiesWith<ecs::SceneConnection>()) {
            if (!e.Has<ecs::SceneConnection, ecs::SceneInfo, ecs::Name>(stagingLock)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(stagingLock);
            if (sceneInfo.scene != *this) continue;

            auto &name = e.Get<ecs::Name>(stagingLock);
            liveConnection = ecs::EntityRef(name).Get(liveLock);
            if (liveConnection.Has<ecs::SceneConnection, ecs::TransformSnapshot>(liveLock)) {
                auto &connection = liveConnection.Get<ecs::SceneConnection>(liveLock);
                if (connection.scenes.count(data->path)) {
                    stagingConnection = e;
                    break;
                }
            }
        }
        if (stagingConnection.Has<ecs::TransformTree>(stagingLock) &&
            liveConnection.Has<ecs::TransformSnapshot>(liveLock)) {
            auto &liveTransform = liveConnection.Get<ecs::TransformSnapshot>(liveLock).globalPose;
            auto &stagingTree = stagingConnection.Get<ecs::TransformTree>(stagingLock);
            auto stagingTransform = stagingTree.GetGlobalTransform(stagingLock);
            glm::quat deltaRotation = liveTransform.GetRotation() * glm::inverse(stagingTransform.GetRotation());
            glm::vec3 deltaPos = liveTransform.GetPosition() - deltaRotation * stagingTransform.GetPosition();
            auto rootTransform = ecs::Transform(deltaPos, deltaRotation);

            auto stagingSceneId = data->sceneEntity.Get(stagingLock);
            Assertf(stagingSceneId,
                "Scene::UpdateSceneProperties scene missing staging scene entity: %s / %s",
                data->name,
                data->sceneEntity.Name().String());
            stagingSceneId.Get<ecs::SceneProperties>(stagingLock).rootTransform = rootTransform;
        }
    }
} // namespace sp
