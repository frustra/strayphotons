#include "game/Scene.hh"

#include "core/Tracing.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/SceneImpl.hh"
#include "game/SceneManager.hh"

namespace sp {
    ecs::Entity Scene::NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        const std::shared_ptr<Scene> &scene,
        ecs::Name entityName) {
        Assertf(ecs::IsStaging(stagingLock), "Scene::NewSystemEntity must be called with a staging lock");
        Assertf(scene, "Scene::NewSystemEntity called with null scene: %s", entityName.String());
        Assertf(scene->priority == ecs::ScenePriority::System,
            "Scene::NewSystemEntity called on non-system scene: %s",
            scene->name);
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
        entity.Set<ecs::SceneInfo>(stagingLock, entity, scene, scene->properties);
        entity.Set<ecs::Name>(stagingLock, entityName);
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    ecs::Entity Scene::NewRootEntity(ecs::Lock<ecs::AddRemove> lock,
        const std::shared_ptr<Scene> &scene,
        std::string relativeName) {
        if (!scene) {
            Errorf("Invalid root entity scene: %s", relativeName);
            return ecs::Entity();
        }

        ecs::Name entityName(relativeName, ecs::Name(scene->name, ""));
        if (entityName) {
            if (!ValidateEntityName(entityName)) {
                Errorf("Invalid root entity name: %s", entityName.String());
                return ecs::Entity();
            }

            if (GetStagingEntity(entityName)) {
                Errorf("Duplicate root entity name: %s", entityName.String());
                return ecs::Entity();
            }
        } else {
            entityName = GenerateEntityName(entityName);
        }

        if (!entityName) {
            Errorf("Invalid root entity name in scene %s: %s", scene->name, relativeName);
            return ecs::Entity();
        } else if (namedEntities.count(entityName) > 0) {
            Errorf("Duplicate root entity name: %s", entityName.String());
            return ecs::Entity();
        }

        auto entity = lock.NewEntity();
        entity.Set<ecs::SceneInfo>(lock, entity, scene, scene->properties);
        entity.Set<ecs::Name>(lock, entityName);
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    ecs::Entity Scene::NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        ecs::Entity prefabRoot,
        size_t prefabScriptId,
        std::string relativeName,
        ecs::Name scope) {
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
        auto &newSceneInfo = entity.Set<ecs::SceneInfo>(stagingLock, entity, prefabRoot, prefabScriptId, rootSceneInfo);
        if (existing) {
            Assertf(existing.Has<ecs::SceneInfo>(stagingLock),
                "Expected existing staging entity to have SceneInfo: %s",
                entityName.String());

            auto &existingSceneInfo = existing.Get<ecs::SceneInfo>(stagingLock);
            newSceneInfo.SetLiveId(stagingLock, existingSceneInfo.liveId);
            existingSceneInfo.InsertWithPriority(stagingLock, newSceneInfo);
        } else {
            namedEntities.emplace(entityName, entity);
            references.emplace_back(entityName, entity);
        }

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
        if (rootSceneInfo.Remove(stagingLock, ent)) {
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

    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live,
        bool resetLive) {
        ZoneScoped;
        ZoneStr(name);
        for (auto e : live.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(live)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(live);
            if (sceneInfo.scene.lock().get() != this) continue;
            Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");

            if (!sceneInfo.rootStagingId.Has<ecs::SceneInfo>(staging)) e.Destroy(live);
        }
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;
            if (sceneInfo.rootStagingId != e) {
                // Skip entities that aren't the root staging id
                continue;
            }

            if (!e.Has<ecs::Name>(staging)) {
                Errorf("Scene contains unnamed entity: %s %s", name, ecs::ToString(staging, e));
                continue;
            }
            auto &entityName = e.Get<const ecs::Name>(staging);

            if (sceneInfo.liveId.Exists(live)) {
                sceneInfo.SetLiveId(staging, sceneInfo.liveId);
                sceneInfo.liveId.Set<ecs::SceneInfo>(live, sceneInfo.FlattenInfo(staging));
                continue;
            }

            // Find matching named entity in live scene
            sceneInfo.liveId = ecs::EntityRef(entityName).Get(live);
            if (sceneInfo.liveId.Exists(live)) {
                // Entity overlaps with another scene
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                liveSceneInfo.InsertWithPriority(staging, sceneInfo);
                sceneInfo.SetLiveId(staging, sceneInfo.liveId);
                liveSceneInfo = liveSceneInfo.FlattenInfo(staging);
            } else {
                // No entity exists in the live scene
                sceneInfo.liveId = live.NewEntity();
                sceneInfo.liveId.Set<ecs::Name>(live, entityName);
                sceneInfo.SetLiveId(staging, sceneInfo.liveId);
                sceneInfo.liveId.Set<ecs::SceneInfo>(live, sceneInfo.FlattenInfo(staging));
                ecs::GetEntityRefs().Set(entityName, e);
                ecs::GetEntityRefs().Set(entityName, sceneInfo.liveId);
            }
        }
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<const ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;
            if (sceneInfo.rootStagingId == e) {
                // Apply the entity if it is the root staging-id
                scene::BuildAndApplyEntity(ecs::Lock<ecs::ReadAll>(staging), live, e, resetLive);
            }
        }
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(staging)) continue;
            auto &sceneInfo = e.Get<const ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;
            if (!sceneInfo.liveId.Has<ecs::TransformTree>(live)) continue;

            auto transform = sceneInfo.liveId.Get<ecs::TransformTree>(live).GetGlobalTransform(live);
            sceneInfo.liveId.Set<ecs::TransformSnapshot>(live, transform);
        }
        active = true;
    }

    void Scene::RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live) {
        ZoneScoped;
        ZoneStr(name);
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(staging)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;

            if (sceneInfo.liveId) {
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                if (sceneInfo.Remove(staging, e)) {
                    // No more staging entities, remove the live id.
                    sceneInfo.liveId.Destroy(live);
                } else {
                    sceneInfo.liveId.Set<ecs::SceneInfo>(live, sceneInfo.FlattenInfo(staging));
                    scene::BuildAndApplyEntity(ecs::Lock<ecs::ReadAll>(staging), live, sceneInfo.rootStagingId, false);
                }
            }
            e.Destroy(staging);
        }
        for (auto &e : live.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(live)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(live);
            if (sceneInfo.scene.lock().get() != this) continue;
            Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");

            if (!sceneInfo.rootStagingId) e.Destroy(live);
        }
        active = false;
    }

    void Scene::UpdateRootTransform() {
        auto stagingLock =
            ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::TransformTree>, ecs::Write<ecs::SceneInfo>>();
        auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneConnection, ecs::TransformSnapshot>>();

        ecs::Entity liveConnection, stagingConnection;
        for (auto &e : stagingLock.EntitiesWith<ecs::SceneConnection>()) {
            if (!e.Has<ecs::SceneConnection, ecs::SceneInfo, ecs::Name>(stagingLock)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(stagingLock);
            auto scenePtr = sceneInfo.scene.lock();
            if (scenePtr.get() != this) continue;

            auto &name = e.Get<ecs::Name>(stagingLock);
            liveConnection = ecs::EntityRef(name).Get(liveLock);
            if (liveConnection.Has<ecs::SceneConnection, ecs::TransformSnapshot>(liveLock)) {
                auto &connection = liveConnection.Get<ecs::SceneConnection>(liveLock);
                if (connection.scenes.count(this->name)) {
                    stagingConnection = e;
                    break;
                }
            }
        }
        if (stagingConnection.Has<ecs::TransformTree>(stagingLock) &&
            liveConnection.Has<ecs::TransformSnapshot>(liveLock)) {
            auto &liveTransform = liveConnection.Get<ecs::TransformSnapshot>(liveLock);
            auto &stagingTree = stagingConnection.Get<ecs::TransformTree>(stagingLock);
            auto stagingTransform = stagingTree.GetGlobalTransform(stagingLock);
            glm::quat deltaRotation = liveTransform.GetRotation() * glm::inverse(stagingTransform.GetRotation());
            glm::vec3 deltaPos = liveTransform.GetPosition() - deltaRotation * stagingTransform.GetPosition();
            this->rootTransform = ecs::Transform(deltaPos, deltaRotation);

            if (this->properties) {
                auto properties = *this->properties;
                properties.fixedGravity = this->rootTransform * glm::vec4(properties.fixedGravity, 0.0f);
                properties.gravityTransform = this->rootTransform * properties.gravityTransform;

                auto propertiesPtr = make_shared<ecs::SceneProperties>(properties);
                for (auto &e : stagingLock.EntitiesWith<ecs::SceneInfo>()) {
                    auto &sceneInfo = e.Get<ecs::SceneInfo>(stagingLock);
                    auto scenePtr = sceneInfo.scene.lock();
                    if (scenePtr.get() != this) continue;

                    sceneInfo.properties = propertiesPtr;
                }
            }
        }
    }

    const ecs::Transform &Scene::GetRootTransform() const {
        return rootTransform;
    }
} // namespace sp
