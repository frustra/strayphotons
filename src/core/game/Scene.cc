#include "Scene.hh"

namespace sp {
    void Scene::CopyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> src, ecs::Lock<ecs::AddRemove> dst) {
        for (auto e : src.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(src);
            if (sceneInfo.scene.get() == this) {
                Assert(sceneInfo.stagingId == e, "Expected source entity to match SceneInfo.stagingId");
                if (sceneInfo.liveId && sceneInfo.liveId.Has<ecs::SceneInfo>(dst)) {
                    auto &liveSceneInfo = sceneInfo.liveId.Get<ecs::SceneInfo>(dst);
                    Assert(liveSceneInfo.stagingId == e, "Expected live scene info to match stagingId");
                    Assert(liveSceneInfo.liveId == sceneInfo.liveId, "Expected live scene info to match liveId");
                } else {
                    auto newEnt = dst.NewEntity();
                    sceneInfo.liveId = newEnt;
                    CopyComponent<ecs::SceneInfo>(src, e, dst, newEnt);
                }
            }
        }
        for (auto e : src.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(src);
            if (sceneInfo.scene.get() == this) {
                Assert(sceneInfo.stagingId == e, "Expected source entity to match SceneInfo.stagingId");
                Assert(!!sceneInfo.liveId, "Expected source entity to have valid SceneInfo.liveId");
                CopyAllComponents(ecs::Lock<ecs::ReadAll>(src), e, dst, sceneInfo.liveId);
            }
        }
    }
} // namespace sp
