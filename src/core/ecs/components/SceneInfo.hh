#pragma once

#include "ecs/Ecs.hh"

#include <memory>

namespace sp {
    class Scene;
} // namespace sp

namespace ecs {
    struct SceneInfo {
        // Lower priority scenes will have their components overwritten with higher priority components.
        enum class Priority : int {
            System,
            Scene,
            Player,
            Bindings,
            Override,
        };

        SceneInfo() {}
        SceneInfo(Entity stagingId, Priority priority, const std::shared_ptr<sp::Scene> &scene)
            : stagingId(stagingId), priority(priority), scene(scene) {}
        SceneInfo(Entity liveId, const SceneInfo &sceneInfo)
            : liveId(liveId), priority(sceneInfo.priority), scene(sceneInfo.scene) {}
        SceneInfo(Entity stagingId, Entity prefabStagingId, const SceneInfo &rootSceneInfo)
            : stagingId(stagingId), prefabStagingId(prefabStagingId), priority(rootSceneInfo.priority),
              scene(rootSceneInfo.scene) {}

        // Should be called on the live SceneInfo
        void InsertWithPriority(Lock<Write<SceneInfo>> staging, const SceneInfo &stagingInfo);

        // Should be called on the live SceneInfo
        // Returns true if live SceneInfo should be removed
        bool Remove(Lock<Write<SceneInfo>> staging, const Entity &stagingId);

        Entity liveId;
        Entity stagingId, nextStagingId;
        Entity prefabStagingId;
        Priority priority = Priority::Scene;
        std::weak_ptr<sp::Scene> scene;
    };
} // namespace ecs
