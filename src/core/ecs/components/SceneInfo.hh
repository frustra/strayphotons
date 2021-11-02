#pragma once

namespace sp {
    class Scene;
}

namespace ecs {
    struct SceneInfo {
        SceneInfo() {}
        SceneInfo(Tecs::Entity stagingId, const std::shared_ptr<sp::Scene> &scene)
            : stagingId(stagingId), scene(scene) {}
        SceneInfo(Tecs::Entity liveId, const SceneInfo &sceneInfo) : liveId(liveId), scene(sceneInfo.scene) {}
        SceneInfo(const std::shared_ptr<sp::Scene> &scene) : scene(scene) {}

        inline bool operator==(const SceneInfo &other) const {
            return scene == other.scene;
        }

        Tecs::Entity liveId, stagingId, nextStagingId;
        std::shared_ptr<sp::Scene> scene;
    };
} // namespace ecs
