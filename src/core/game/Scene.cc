#include "Scene.hh"

namespace sp {
    void Scene::ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging,
        ecs::Lock<ecs::AddRemove> live) {
        Logf("Applying scene: %s", sceneName);
        // Update SceneInfo.liveId of all new entities
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.get() == this) {
                Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");
                if (sceneInfo.liveId) {
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
                    auto &name = e.Get<const ecs::Name>(staging);
                    sceneInfo.liveId = ecs::EntityWith<ecs::Name>(live, name);
                    if (sceneInfo.liveId) {
                        Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                        auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);
                        Assert(liveSceneInfo.stagingId != e, "Unexpected matching stagingId");
                        Assert(liveSceneInfo.stagingId.Has<ecs::SceneInfo>(staging),
                            "Expected stagingId to have SceneInfo");
                        Logf("Entity collision: %s, (existing: %d, new: %d)",
                            name,
                            liveSceneInfo.scene->priority,
                            sceneInfo.scene->priority);
                        if (sceneInfo.scene->priority > liveSceneInfo.scene->priority) {
                            Logf("Overriding %s in %s with %s scene", name, liveSceneInfo.scene->sceneName, sceneName);
                            // Apply new entity on top of existing entity
                            sceneInfo.nextStagingId = liveSceneInfo.stagingId;
                            CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
                        } else {
                            // Existing entity overrides this scene's entity
                            ecs::SceneInfo *prevSceneInfo = &liveSceneInfo.stagingId.Get<ecs::SceneInfo>(staging);
                            auto stagingId = liveSceneInfo.nextStagingId;
                            while (stagingId && stagingId.Has<ecs::SceneInfo>(staging)) {
                                auto &stagingSceneInfo = stagingId.Get<ecs::SceneInfo>(staging);
                                if (sceneInfo.scene->priority > stagingSceneInfo.scene->priority) break;
                                stagingId = stagingSceneInfo.nextStagingId;
                                prevSceneInfo = &stagingSceneInfo;
                            }
                            if (prevSceneInfo->scene.get() == this) {
                                Logf("Found existing override: ", name);
                            } else {
                                Logf("Overriding %s at %s scene", name, prevSceneInfo->scene->sceneName);
                                prevSceneInfo->nextStagingId = e;
                                sceneInfo.nextStagingId = stagingId;
                                CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
                            }
                        }
                    } else {
                        Logf("New entity: %s", name);
                        auto newEnt = live.NewEntity();
                        sceneInfo.liveId = newEnt;
                        CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
                    }
                } else {
                    auto newEnt = live.NewEntity();
                    sceneInfo.liveId = newEnt;
                    CopyComponent<ecs::SceneInfo>(staging, e, live, sceneInfo.liveId);
                }
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

    void Scene::RemoveScene(ecs::Lock<ecs::Read<ecs::Name, ecs::SceneInfo>> staging, ecs::Lock<ecs::AddRemove> live) {
        Logf("Removing scene: %s", sceneName);
        for (auto e : staging.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.scene.get() == this) {
                Assert(sceneInfo.stagingId == e, "Expected staging entity to match SceneInfo.stagingId");
                if (sceneInfo.liveId) {
                    Assert(sceneInfo.liveId.Has<ecs::SceneInfo>(live), "Expected liveId to have SceneInfo");
                    auto &liveSceneInfo = sceneInfo.liveId.Get<const ecs::SceneInfo>(live);
                    if (sceneInfo.liveId.Has<ecs::Name>(live)) {
                        auto &name = sceneInfo.liveId.Get<const ecs::Name>(live);
                        if (liveSceneInfo.stagingId == e) {
                            if (liveSceneInfo.nextStagingId) {
                                Logf("Removing primary entity: %s", name);
                            } else {
                                Logf("Removing only entity: %s", name);
                            }
                        } else if (liveSceneInfo.nextStagingId) {
                            Logf("Removing secondary entity: %s", name);
                        } else {
                            Logf("Removing invalid entity: %s", name);
                        }
                    }
                } else if (e.Has<ecs::Name>(staging)) {
                    auto &name = e.Get<const ecs::Name>(staging);
                    Logf("Removing unused entity: %s", name);
                }
            }
        }
    }
} // namespace sp
