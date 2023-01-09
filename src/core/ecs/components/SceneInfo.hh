#pragma once

#include "ecs/Ecs.hh"
#include "game/SceneRef.hh"

#include <memory>

namespace ecs {
    struct SceneInfo {
        SceneInfo() {}
        SceneInfo(Entity ent, const std::shared_ptr<sp::Scene> &scene);

        SceneInfo(Entity rootStagingId, Entity prefabStagingId, size_t prefabScriptId, const SceneInfo &rootSceneInfo)
            : rootStagingId(rootStagingId), prefabStagingId(prefabStagingId), prefabScriptId(prefabScriptId),
              priority(rootSceneInfo.priority), scene(rootSceneInfo.scene) {
            Assertf(IsStaging(rootStagingId), "Invalid rootStagingId in SceneInfo: %s", std::to_string(rootStagingId));
            Assertf(IsStaging(prefabStagingId),
                "Invalid prefabStagingId in SceneInfo: %s",
                std::to_string(prefabStagingId));
            Assertf(prefabScriptId > 0, "Invalid prefabScriptId in SceneInfo: %s", std::to_string(prefabScriptId));
        }

        // Input rootSceneInfo may reference multiple entities via linked-list
        void InsertWithPriority(Lock<Write<SceneInfo>> staging, const SceneInfo &rootSceneInfo) const;

        void SetLiveId(Lock<Write<SceneInfo>> staging, Entity liveId) const;

        // Returns the remaining rootStagingId, or a null entity if the removed entity was the only one in the list
        Entity Remove(Lock<Write<SceneInfo>> staging, const Entity &stagingId) const;

        Entity liveId;
        // Staging IDs are stored in a singly-linked list, with highest priority first.
        Entity rootStagingId, nextStagingId;

        Entity prefabStagingId;
        size_t prefabScriptId = 0;

        sp::ScenePriority priority = sp::ScenePriority::Scene;
        sp::SceneRef scene;
    };
} // namespace ecs
