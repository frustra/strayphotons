#pragma once

#include "ecs/Ecs.hh"

#include <memory>

namespace sp {
    class Scene;
} // namespace sp

namespace ecs {
    struct SceneProperties;

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
        SceneInfo(Entity ent,
            const std::shared_ptr<sp::Scene> &scene,
            const std::shared_ptr<SceneProperties> &properties);

        SceneInfo(Entity liveId, const SceneInfo &sceneInfo)
            : liveId(liveId), priority(sceneInfo.priority), scene(sceneInfo.scene), properties(sceneInfo.properties) {
            Assertf(IsLive(liveId), "Invalid liveId in SceneInfo: %s", std::to_string(liveId));
        }

        SceneInfo(Entity rootStagingId, Entity prefabStagingId, size_t prefabScriptId, const SceneInfo &rootSceneInfo)
            : rootStagingId(rootStagingId), prefabStagingId(prefabStagingId), prefabScriptId(prefabScriptId),
              priority(rootSceneInfo.priority), scene(rootSceneInfo.scene), properties(rootSceneInfo.properties) {
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

        // Checks if this SceneInfo points to the provided scene
        // Uses a thread-safe implementation without weak_ptr::lock()
        bool FromScene(const std::shared_ptr<sp::Scene> &scene) const {
            return !this->scene.owner_before(scene) && !scene.owner_before(this->scene);
        }

        Entity liveId;
        // Staging IDs are stored in a singly-linked list, with highest priority first.
        Entity rootStagingId, nextStagingId;
        Entity prefabStagingId;
        size_t prefabScriptId = 0;
        ScenePriority priority = ScenePriority::Scene;
        std::weak_ptr<sp::Scene> scene;
        std::shared_ptr<SceneProperties> properties;
    };
} // namespace ecs
