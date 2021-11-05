#include "Scene.hh"

namespace sp {
    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live) {
        Logf("Applying scene: %s", sceneName);
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.get() != this) continue;
            Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");

            // Skip entities that have already been added
            if (sceneInfo.liveId) continue;

            if (e.Has<ecs::Name>(staging)) {
                // Find matching named entity in live scene
                auto &name = e.Get<const ecs::Name>(staging);
                sceneInfo.liveId = ecs::EntityWith<ecs::Name>(live, name);
                if (sceneInfo.liveId) {
                    // Entity overlaps with another scene
                    Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                    auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                    liveSceneInfo.InsertWithPriority(staging, sceneInfo);
                } else {
                    // No entity exists in the live scene
                    Logf("New entity: %s", name);
                    sceneInfo.liveId = live.NewEntity();
                    CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
                    CopyComponent<ecs::Name>(staging, e, live, sceneInfo.liveId);
                }
            } else {
                // No entity exists in the live scene
                sceneInfo.liveId = live.NewEntity();
                CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
            }
        }
        // Apply component changes to match scene priority
        for (auto e : live.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(live);
            if (sceneInfo.scene.get() == this) {
                Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");
                // Apply any entity overrides from other scenes
                auto stagingId = sceneInfo.stagingId;
                while (stagingId.Has<ecs::SceneInfo>(staging)) {
                    CopyAllComponents(ecs::Lock<ecs::ReadAll>(staging), stagingId, live, sceneInfo.liveId);
                    auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(staging);
                    stagingId = stagingInfo.nextStagingId;
                }
                CopyComponent<ecs::SceneInfo>(staging, sceneInfo.stagingId, live, sceneInfo.liveId);
            }
        }
    }

    void Scene::RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live) {
        Logf("Removing scene: %s", sceneName);
        for (auto &e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.get() != this) continue;
            Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");

            if (sceneInfo.liveId) {
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(live);
                if (liveSceneInfo.Remove(staging, e)) sceneInfo.liveId.Destroy(live);
            }
            e.Destroy(staging);
        }
    }
} // namespace sp
