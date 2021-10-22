#include "Scene.hh"

namespace sp {
    void Scene::CopyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> src, ecs::Lock<ecs::AddRemove> dst) {
        for (auto e : src.EntitiesWith<ecs::SceneInfo>()) {
            auto &sceneInfo = e.Get<ecs::SceneInfo>(src);
            if (sceneInfo.scene.get() == this) {
                Assert(sceneInfo.stagingId == e, "Expected source entity to match SceneInfo.stagingId");
                auto newEnt = dst.NewEntity();
                sceneInfo.liveId = newEnt;
                CopyAllComponents(ecs::Lock<ecs::ReadAll>(src), e, dst, newEnt);
            }
        }
    }
} // namespace sp
