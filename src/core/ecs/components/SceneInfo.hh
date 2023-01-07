#pragma once

#include "ecs/Ecs.hh"
#include "game/SceneRef.hh"

#include <memory>

namespace sp {
    struct SceneProperties;
}

namespace ecs {
    // Lower priority scenes will have their components overwritten with higher priority components.
    enum class ScenePriority {
        System, // Lowest priority
        Scene,
        Player,
        Bindings,
        Override, // Highest priority
    };

    struct SceneInfo {
        SceneInfo() {}
        SceneInfo(Entity ent, const std::shared_ptr<sp::Scene> &scene);

        SceneInfo(Entity liveId, const SceneInfo &sceneInfo)
            : liveId(liveId), priority(sceneInfo.priority), scene(sceneInfo.scene) {
            Assertf(IsLive(liveId), "Invalid liveId in SceneInfo: %s", std::to_string(liveId));
        }

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

        // Generate a merged SceneInfo instance for applying to the live ECS
        SceneInfo FlattenInfo(Lock<Read<SceneInfo>> staging) const;

        void SetLiveId(Lock<Write<SceneInfo>> staging, Entity liveId) const;

        // Returns the remaining rootStagingId, or a null entity if the removed entity was the only one in the list
        Entity Remove(Lock<Write<SceneInfo>> staging, const Entity &stagingId) const;

        Entity liveId;
        // Staging IDs are stored in a singly-linked list, with highest priority first.
        Entity rootStagingId, nextStagingId;
        Entity prefabStagingId;
        size_t prefabScriptId = 0;
        ScenePriority priority = ScenePriority::Scene;
        sp::SceneRef scene;
        std::shared_ptr<sp::SceneProperties> properties;
    };
} // namespace ecs
