#pragma once

#include "ecs/Ecs.hh"

#include <memory>

namespace sp {
    class Scene;
} // namespace sp

namespace ecs {
    struct SceneProperties;

    struct SceneInfo {
        // Lower priority scenes will have their components overwritten with higher priority components.
        enum class Priority : int {
            System, // Lowest priority
            Scene,
            Player,
            Bindings,
            Override, // Highest priority
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
                rootStagingId = ent;
            } else {
                Abortf("Invalid SceneInfo entity: %s", std::to_string(ent));
            }
        }

        SceneInfo(Entity liveId, const SceneInfo &sceneInfo)
            : liveId(liveId), priority(sceneInfo.priority), scene(sceneInfo.scene), properties(sceneInfo.properties) {
            Assertf(IsLive(liveId), "Invalid liveId in SceneInfo: %s", std::to_string(liveId));
        }

        SceneInfo(Entity rootStagingId, Entity prefabStagingId, const SceneInfo &rootSceneInfo)
            : rootStagingId(rootStagingId), prefabStagingId(prefabStagingId), priority(rootSceneInfo.priority),
              scene(rootSceneInfo.scene), properties(rootSceneInfo.properties) {
            Assertf(IsStaging(rootStagingId), "Invalid rootStagingId in SceneInfo: %s", std::to_string(rootStagingId));
            Assertf(IsStaging(prefabStagingId),
                "Invalid prefabStagingId in SceneInfo: %s",
                std::to_string(prefabStagingId));
        }

        // Should be called on the live SceneInfo
        // Input stagingInfo may reference multiple entities via linked-list
        void InsertWithPriority(Lock<Write<SceneInfo>> staging, const SceneInfo &stagingInfo);

        // Should be called on the live SceneInfo
        // Returns true if live SceneInfo should be removed
        bool Remove(Lock<Write<SceneInfo>> staging, const Entity &stagingId);

        Entity liveId;
        // Staging IDs are stored in a singly-linked list, with highest priority first.
        Entity rootStagingId, nextStagingId;
        Entity prefabStagingId;
        Priority priority = Priority::Scene;
        std::weak_ptr<sp::Scene> scene;
        std::shared_ptr<SceneProperties> properties;
    };
} // namespace ecs
