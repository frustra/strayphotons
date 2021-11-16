#include "Scene.hh"

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

    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live) {
        Logf("Applying scene: %s", sceneName);
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.get() != this) continue;
            Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");

            // Skip entities that have already been added
            if (!sceneInfo.liveId) {
                // Find matching named entity in live scene
                if (e.Has<ecs::Name>(staging)) {
                    auto &name = e.Get<const ecs::Name>(staging);
                    sceneInfo.liveId = ecs::EntityWith<ecs::Name>(live, name);
                    if (sceneInfo.liveId) {
                        // Entity overlaps with another scene
                        Logf("Merging entity: %s", name);
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

            ApplyComponentsByPriority(staging, live, e);
        }
    }

    void Scene::RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live) {
        Logf("Removing scene: %s", sceneName);
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            if (!e.Has<ecs::SceneInfo>(staging)) continue;
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.get() != this) continue;
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
            if (sceneInfo.scene.get() != this) continue;
            Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");

            if (!sceneInfo.stagingId) e.Destroy(live);
        }
    }
} // namespace sp
