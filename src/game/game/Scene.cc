#include "game/Scene.hh"

#include "core/Tracing.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/SceneImpl.hh"
#include "game/SceneManager.hh"

namespace sp {
    void RebuildComponentsByPriority(ecs::Lock<ecs::ReadAll> staging,
        ecs::Lock<ecs::AddRemove> live,
        ecs::Entity e,
        bool resetLive = false) {
        Assert(e.Has<ecs::SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
        Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);

        std::vector<ecs::Entity> stagingIds;
        auto stagingId = liveSceneInfo.stagingId;
        while (stagingId.Has<ecs::SceneInfo>(staging)) {
            stagingIds.emplace_back(stagingId);

            auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(staging);
            stagingId = stagingInfo.nextStagingId;
        }

        if (resetLive) scene::RemoveAllComponents(live, sceneInfo.liveId);

        while (!stagingIds.empty()) {
            scene::ApplyAllComponents(ecs::Lock<ecs::ReadAll>(staging), stagingIds.back(), live, sceneInfo.liveId);
            stagingIds.pop_back();
        }
        scene::RemoveDanglingComponents(ecs::Lock<ecs::ReadAll>(staging), live, sceneInfo.liveId);
    }

    void ApplyComponentsByPriority(ecs::Lock<ecs::ReadAll> staging, ecs::Lock<ecs::AddRemove> live, ecs::Entity e) {
        Assert(e.Has<ecs::SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        auto &rootSceneInfo = e.Get<ecs::SceneInfo>(staging);
        Assert(rootSceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        Assert(rootSceneInfo.stagingId == e, "Expected supplied entity to be the root stagingId");
        auto &liveSceneInfo = rootSceneInfo.liveId.Get<const ecs::SceneInfo>(live);
        if (liveSceneInfo.stagingId == e) {
            while (e.Has<ecs::SceneInfo>(staging)) {
                // Entity is the linked-list root, which can be applied directly.
                scene::ApplyAllComponents(ecs::Lock<ecs::ReadAll>(staging), e, live, rootSceneInfo.liveId);
                e = e.Get<ecs::SceneInfo>(staging).nextStagingId;
            }
        } else {
            RebuildComponentsByPriority(staging, live, e);
        }
    }

    ecs::Entity Scene::NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        const std::shared_ptr<Scene> &scene,
        ecs::Name entityName) {
        Assertf(ecs::IsStaging(stagingLock), "Scene::NewSystemEntity must be called with a staging lock");
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
        entity.Set<ecs::SceneInfo>(stagingLock, entity, ecs::SceneInfo::Priority::System, scene, scene->properties);
        entity.Set<ecs::Name>(stagingLock, entityName);
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    ecs::Entity Scene::NewRootEntity(ecs::Lock<ecs::AddRemove> lock,
        const std::shared_ptr<Scene> &scene,
        ecs::SceneInfo::Priority priority,
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
        entity.Set<ecs::SceneInfo>(lock, entity, priority, scene, scene->properties);
        entity.Set<ecs::Name>(lock, entityName);
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    ecs::Entity Scene::NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        ecs::Entity prefabRoot,
        std::string relativeName,
        ecs::Name scope) {
        Assertf(ecs::IsStaging(stagingLock), "Scene::NewPrefabEntity must be called with a staging lock");

        ecs::Entity existing;
        ecs::Name entityName;
        if (!relativeName.empty()) {
            entityName = ecs::Name(relativeName, scope);
            if (relativeName != "scoperoot" && !ValidateEntityName(entityName)) {
                Errorf("Invalid prefab entity name: %s (scope: %s)", relativeName, scope.String());
                return ecs::Entity();
            }

            existing = GetStagingEntity(entityName);
            if (existing.Has<ecs::SceneInfo>(stagingLock)) {
                auto &sceneInfo = existing.Get<ecs::SceneInfo>(stagingLock);
                Assertf(sceneInfo.stagingId.Has<ecs::SceneInfo>(stagingLock), "Expected root entity to have SceneInfo");
                existing = sceneInfo.stagingId; // Point to the current root entity.
            }
        }

        Assertf(prefabRoot.Has<ecs::SceneInfo>(stagingLock),
            "Prefab root %s does not have SceneInfo",
            ecs::ToString(stagingLock, prefabRoot));

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
        auto &newSceneInfo = entity.Set<ecs::SceneInfo>(stagingLock, entity, prefabRoot, rootSceneInfo);
        // Insert the prefab entity at the linked-list root (i.e. lowest priority)
        newSceneInfo.nextStagingId = existing;
        auto nextId = existing;
        while (nextId.Has<ecs::SceneInfo>(stagingLock)) {
            auto &nextSceneInfo = nextId.Get<ecs::SceneInfo>(stagingLock);
            nextSceneInfo.stagingId = entity;
            nextId = nextSceneInfo.nextStagingId;
        }

        if (!existing) {
            namedEntities.emplace(entityName, entity);
            references.emplace_back(entityName, entity);
        }
        return entity;
    }

    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live,
        bool resetLive) {
        ZoneScoped;
        ZoneStr(name);
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;
            if (sceneInfo.stagingId != e) {
                // Skip entities that are sub-entities of another.
                continue;
            }

            if (!e.Has<ecs::Name>(staging)) {
                Errorf("Scene contains unnamed entity: %s %s", name, ecs::ToString(staging, e));
                continue;
            }
            auto &entityName = e.Get<const ecs::Name>(staging);

            // Skip entities that have already been added
            if (sceneInfo.liveId.Exists(live)) continue;

            // Find matching named entity in live scene
            sceneInfo.liveId = ecs::EntityRef(entityName).Get(live);
            if (sceneInfo.liveId.Exists(live)) {
                // Entity overlaps with another scene
                ZoneScopedN("MergeEntity");
                ZoneStr(entityName.String());
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                liveSceneInfo.InsertWithPriority(staging, sceneInfo);
            } else {
                // No entity exists in the live scene
                sceneInfo.liveId = live.NewEntity();
                sceneInfo.liveId.Set<ecs::SceneInfo>(live, sceneInfo);
                sceneInfo.liveId.Set<ecs::Name>(live, entityName);
                ecs::GetEntityRefs().Set(entityName, e);
                ecs::GetEntityRefs().Set(entityName, sceneInfo.liveId);
            }
        }
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<const ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;
            if (sceneInfo.stagingId != e) {
                // Sub-entities don't need to be applied
                e.Get<ecs::SceneInfo>(staging).liveId = sceneInfo.stagingId.Get<const ecs::SceneInfo>(staging).liveId;
            } else {
                if (resetLive) {
                    RebuildComponentsByPriority(staging, live, e, true);
                } else {
                    ApplyComponentsByPriority(staging, live, e);
                }
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
            auto scenePtr = sceneInfo.scene.lock();
            if (scenePtr.get() != this) continue;

            if (sceneInfo.liveId) {
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                if (liveSceneInfo.Remove(staging, e)) {
                    // No more staging entities, remove the live id.
                    sceneInfo.liveId.Destroy(live);
                } else {
                    RebuildComponentsByPriority(staging, live, liveSceneInfo.stagingId);
                }
            }
            e.Destroy(staging);
        }
        for (auto &e : live.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(live)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(live);
            auto scenePtr = sceneInfo.scene.lock();
            if (scenePtr.get() != this) continue;
            Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");

            if (!sceneInfo.stagingId) e.Destroy(live);
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
