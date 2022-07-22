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

        SceneInfo(Entity ent, Priority priority, const std::shared_ptr<sp::Scene> &scene)
            : priority(priority), scene(scene) {
            if (IsLive(ent)) {
                liveId = ent;
            } else {
                stagingId = ent;
            }
        }

        SceneInfo(Entity liveId, const SceneInfo &sceneInfo)
            : liveId(liveId), priority(sceneInfo.priority), scene(sceneInfo.scene) {
            Assertf(IsLive(liveId), "Invalid liveId in SceneInfo: %s", std::to_string(liveId));
        }

        SceneInfo(Entity stagingId, Entity prefabStagingId, const SceneInfo &rootSceneInfo)
            : stagingId(stagingId), prefabStagingId(prefabStagingId), priority(rootSceneInfo.priority),
              scene(rootSceneInfo.scene) {
            Assertf(!IsLive(stagingId), "Invalid stagingId in SceneInfo: %s", std::to_string(stagingId));
            Assertf(!IsLive(prefabStagingId),
                "Invalid prefabStagingId in SceneInfo: %s",
                std::to_string(prefabStagingId));
        }

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
