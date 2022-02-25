#include "Scene.hh"

#include "core/Tracing.hh"
#include "game/SceneManager.hh"

namespace sp {
    void RebuildComponentsByPriority(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live,
        Tecs::Entity e) {

        Assert(e.Has<ecs::SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        auto &sceneInfo = e.Get<const ecs::SceneInfo>(staging);
        Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);

        std::vector<Tecs::Entity> stagingIds;
        auto stagingId = liveSceneInfo.stagingId;
        while (stagingId.Has<ecs::SceneInfo>(staging)) {
            stagingIds.emplace_back(stagingId);

            auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(staging);
            stagingId = stagingInfo.nextStagingId;
        }
        while (!stagingIds.empty()) {
            Scene::CopyAllComponents(ecs::Lock<ecs::ReadAll>(staging), stagingIds.back(), live, sceneInfo.liveId);
            stagingIds.pop_back();
        }
        Scene::RemoveDanglingComponents(ecs::Lock<ecs::ReadAll>(staging), live, sceneInfo.liveId);
    }

    void ApplyComponentsByPriority(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live,
        Tecs::Entity e) {

        Assert(e.Has<ecs::SceneInfo>(staging), "Expected entity to have valid SceneInfo");
        auto &sceneInfo = e.Get<const ecs::SceneInfo>(staging);
        Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have valid SceneInfo");
        auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);

        if (liveSceneInfo.stagingId == e) {
            // Entity is the linked-list root, which can be applied directly.
            Scene::CopyAllComponents(ecs::Lock<ecs::ReadAll>(staging), e, live, sceneInfo.liveId);
            return;
        }

        RebuildComponentsByPriority(staging, live, e);
    }

    Scene::~Scene() {
        Assertf(!active, "%s scene destroyed while active: %s", type, name);
    }

    Tecs::Entity Scene::NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        const std::shared_ptr<Scene> &scene,
        std::string entityName) {
        if (!entityName.empty() && namedEntities.count(entityName) > 0) return namedEntities[entityName];

        auto entity = stagingLock.NewEntity();
        entity.Set<ecs::SceneInfo>(stagingLock, entity, ecs::SceneInfo::Priority::System, scene);
        if (!entityName.empty()) {
            entity.Set<ecs::Name>(stagingLock, entityName);
            namedEntities.emplace(entityName, entity);
        }
        return entity;
    }

    Tecs::Entity Scene::NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
        Tecs::Entity prefabRoot,
        std::string entityName) {
        if (!entityName.empty() && namedEntities.count(entityName) > 0) return namedEntities[entityName];

        Assertf(prefabRoot.Has<ecs::SceneInfo>(stagingLock),
            "Prefab root %s does not have SceneInfo",
            ecs::ToString(stagingLock, prefabRoot));
        auto &rootSceneInfo = prefabRoot.Get<const ecs::SceneInfo>(stagingLock);

        auto entity = stagingLock.NewEntity();
        entity.Set<ecs::SceneInfo>(stagingLock, entity, prefabRoot, rootSceneInfo);
        if (!entityName.empty()) {
            if (!starts_with(entityName, "global.") && !starts_with(entityName, this->name + ".")) {
                entityName = this->name + "." + entityName;
            }
            entity.Set<ecs::Name>(stagingLock, entityName);
            namedEntities.emplace(entityName, entity);
        }
        return entity;
    }

    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live) {
        ZoneScoped;
        ZoneStr(name);
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;
            Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");

            // Skip entities that have already been added
            if (!sceneInfo.liveId) {
                // Find matching named entity in live scene
                if (e.Has<ecs::Name>(staging)) {
                    auto &entityName = e.Get<const ecs::Name>(staging);
                    sceneInfo.liveId = ecs::EntityWith<ecs::Name>(live, entityName);
                    if (sceneInfo.liveId) {
                        // Entity overlaps with another scene
                        ZoneScopedN("MergeEntity");
                        ZoneStr(entityName);
                        Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                        auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                        liveSceneInfo.InsertWithPriority(staging, sceneInfo);
                    }
                }
                if (!sceneInfo.liveId) {
                    // No entity exists in the live scene
                    sceneInfo.liveId = live.NewEntity();
                    CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
                    CopyComponent<ecs::Name>(staging, e, live, sceneInfo.liveId);
                }
            }
        }
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.lock().get() != this) continue;

            ApplyComponentsByPriority(staging, live, e);
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
} // namespace sp
