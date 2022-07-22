#include "Scene.hh"

#include "core/Tracing.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/SceneImpl.hh"
#include "game/SceneManager.hh"

namespace sp {
    void RebuildComponentsByPriority(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live,
        ecs::Entity e) {

        Assert(e.Has<ecs::SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        auto &sceneInfo = e.Get<const ecs::SceneInfo>(staging);
        Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);

        std::vector<ecs::Entity> stagingIds;
        auto stagingId = liveSceneInfo.stagingId;
        while (stagingId.Has<ecs::SceneInfo>(staging)) {
            stagingIds.emplace_back(stagingId);

            auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(staging);
            stagingId = stagingInfo.nextStagingId;
        }
        while (!stagingIds.empty()) {
            scene::ApplyAllComponents(ecs::Lock<ecs::ReadAll>(staging), stagingIds.back(), live, sceneInfo.liveId);
            stagingIds.pop_back();
        }
        scene::RemoveDanglingComponents(ecs::Lock<ecs::ReadAll>(staging), live, sceneInfo.liveId);
    }

    void ApplyComponentsByPriority(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live,
        ecs::Entity e) {

        Assert(e.Has<ecs::SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        auto &sceneInfo = e.Get<const ecs::SceneInfo>(staging);
        Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);

        if (liveSceneInfo.stagingId == e) {
            // Entity is the linked-list root, which can be applied directly.
            scene::ApplyAllComponents(ecs::Lock<ecs::ReadAll>(staging), e, live, sceneInfo.liveId);
        } else {
            RebuildComponentsByPriority(staging, live, e);
        }
    }

    Scene::~Scene() {
        Assertf(!active, "%s scene destroyed while active: %s", type, name);
    }

    ecs::Entity Scene::NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        const std::shared_ptr<Scene> &scene,
        ecs::Name entityName) {
        Assertf(!ecs::IsLive(stagingLock), "Scene::NewSystemEntity must be called with a staging lock");
        if (entityName) {
            if (!ValidateEntityName(entityName)) {
                Errorf("Invalid system entity name: %s", entityName.String());
                return ecs::Entity();
            }

            auto existing = GetStagingEntity(entityName);
            if (existing) return existing;
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
        entity.Set<ecs::SceneInfo>(stagingLock, entity, ecs::SceneInfo::Priority::System, scene);
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

            auto existing = GetStagingEntity(entityName);
            if (existing) return existing;
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
        entity.Set<ecs::SceneInfo>(lock, entity, priority, scene);
        entity.Set<ecs::Name>(lock, entityName);
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    ecs::Entity Scene::NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        ecs::Entity prefabRoot,
        std::string relativeName,
        ecs::Name scope) {
        Assertf(!ecs::IsLive(stagingLock), "Scene::NewPrefabEntity must be called with a staging lock");

        ecs::Name entityName;
        if (!relativeName.empty()) {
            entityName = ecs::Name(relativeName, scope);
            if (!ValidateEntityName(entityName)) {
                Errorf("Invalid prefab entity name: %s (scope: %s)", relativeName, scope.String());
                return ecs::Entity();
            }

            auto existing = GetStagingEntity(entityName);
            if (existing) return existing;
        }

        Assertf(prefabRoot.Has<ecs::SceneInfo>(stagingLock),
            "Prefab root %s does not have SceneInfo",
            ecs::ToString(stagingLock, prefabRoot));
        auto &rootSceneInfo = prefabRoot.Get<const ecs::SceneInfo>(stagingLock);

        if (!entityName) entityName = GenerateEntityName(scope);
        if (!entityName) {
            Errorf("Invalid root entity name: %s", entityName.String());
            return ecs::Entity();
        } else if (namedEntities.count(entityName) > 0) {
            Errorf("Duplicate prefab entity name: %s", entityName.String());
            return ecs::Entity();
        }

        auto entity = stagingLock.NewEntity();
        entity.Set<ecs::SceneInfo>(stagingLock, entity, prefabRoot, rootSceneInfo);
        entity.Set<ecs::Name>(stagingLock, entityName);
        namedEntities.emplace(entityName, entity);
        references.emplace_back(entityName, entity);
        return entity;
    }

    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live) {
        ZoneScoped;
        ZoneStr(name);
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;
            Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");

            // Skip entities that have already been added
            if (sceneInfo.liveId) continue;

            if (!e.Has<ecs::Name>(staging)) {
                Errorf("Scene contains unnamed entity: %s %s", name, ecs::ToString(staging, e));
                continue;
            }
            auto &entityName = e.Get<const ecs::Name>(staging);

            // Find matching named entity in live scene
            sceneInfo.liveId = ecs::EntityWith<ecs::Name>(live, entityName);
            if (sceneInfo.liveId) {
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
                ecs::GEntityRefs.Set(entityName, e);
                ecs::GEntityRefs.Set(entityName, sceneInfo.liveId);
            }
        }
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;

            ApplyComponentsByPriority(staging, live, e);
        }
        for (auto e : staging.EntitiesWith<ecs::TransformTree>()) {
            if (!e.Has<ecs::TransformTree, ecs::SceneInfo>(staging)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
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
            if (scenePtr != nullptr && scenePtr.get() != this) continue;
            Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");

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
            if (scenePtr != nullptr && scenePtr.get() != this) continue;
            Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");

            if (!sceneInfo.stagingId) e.Destroy(live);
        }
        active = false;
    }

    ecs::Name Scene::GenerateEntityName(const ecs::Name &prefix) {
        unnamedEntityCount++;
        if (!prefix.entity.empty()) {
            return ecs::Name(prefix.scene, prefix.entity + ".entity" + std::to_string(unnamedEntityCount));
        } else {
            return ecs::Name(prefix.scene, "entity" + std::to_string(unnamedEntityCount));
        }
    }

    bool Scene::ValidateEntityName(const ecs::Name &name) const {
        if (!name) return false;
        if (starts_with(name.entity, "entity")) {
            return std::find_if(name.entity.begin() + 7, name.entity.end(), [](char ch) {
                return !std::isdigit(ch);
            }) != name.entity.end();
        }
        return true;
    }
} // namespace sp
