#include "Scene.hh"

namespace sp {
    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live) {
        Logf("Applying scene: %s", sceneName);
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.get() != this) continue;
            Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");

            if (sceneInfo.liveId) {
                // Entity already exists in live scene
                Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                if (e.Has<ecs::Name>(staging)) {
                    Assert(sceneInfo.liveId.Has<ecs::Name>(live), "Expected liveId to have a name");
                    auto &name = e.Get<const ecs::Name>(staging);
                    Logf("Updating existing entity: %s", name);
                }
                auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);
                Assert(liveSceneInfo.stagingId == e, "Expected live scene info to match stagingId");
                Assert(liveSceneInfo.liveId == sceneInfo.liveId, "Expected live scene info to match liveId");
            } else if (e.Has<ecs::Name>(staging)) {
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
                    auto newEnt = live.NewEntity();
                    sceneInfo.liveId = newEnt;
                    CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
                    CopyComponent<ecs::Name>(staging, e, live, sceneInfo.liveId);
                }
            } else {
                // No entity exists in the live scene
                auto newEnt = live.NewEntity();
                sceneInfo.liveId = newEnt;
                CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
            }
        }
        // Apply component changes to match scene priority
        for (auto e : live.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(live);
            if (sceneInfo.scene.get() == this) {
                Assert(sceneInfo.liveId == e, "Expected live entity to match SceneInfo.liveId");
                CopyAllComponents(ecs::Lock<ecs::ReadAll>(staging), sceneInfo.stagingId, live, sceneInfo.liveId);
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
