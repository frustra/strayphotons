#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/SceneProperties.hh"

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

        SceneInfo(Entity ent,
            Priority priority,
            const std::shared_ptr<sp::Scene> &scene,
            const std::shared_ptr<SceneProperties> &properties)
            : priority(priority), scene(scene), properties(properties) {
            if (IsLive(ent)) {
                liveId = ent;
            } else if (IsStaging(ent)) {
                stagingId = ent;
            } else {
                Abortf("Invalid SceneInfo entity: %s", std::to_string(ent));
            }
        }

        SceneInfo(Entity liveId, const SceneInfo &sceneInfo)
            : liveId(liveId), priority(sceneInfo.priority), scene(sceneInfo.scene), properties(sceneInfo.properties) {
            Assertf(IsLive(liveId), "Invalid liveId in SceneInfo: %s", std::to_string(liveId));
        }

        SceneInfo(Entity stagingId, Entity prefabStagingId, const SceneInfo &rootSceneInfo)
            : stagingId(stagingId), prefabStagingId(prefabStagingId), priority(rootSceneInfo.priority),
              scene(rootSceneInfo.scene), properties(rootSceneInfo.properties) {
            Assertf(IsStaging(stagingId), "Invalid stagingId in SceneInfo: %s", std::to_string(stagingId));
            Assertf(IsStaging(prefabStagingId),
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
        std::shared_ptr<SceneProperties> properties;
    };
} // namespace ecs
