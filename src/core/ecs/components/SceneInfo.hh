#pragma once

#include "ecs/Ecs.hh"

#include <memory>

namespace sp {
    class Scene;
}

namespace ecs {
    struct SceneInfo {
        enum class Priority : int {
            System,
            Scene,
            Player,
            Bindings,
            Override,
        };

        SceneInfo() {}
        SceneInfo(Tecs::Entity stagingId, Priority priority, const std::shared_ptr<sp::Scene> &scene)
            : stagingId(stagingId), priority(priority), scene(scene) {}
        SceneInfo(Tecs::Entity liveId, const SceneInfo &sceneInfo)
            : liveId(liveId), priority(sceneInfo.priority), scene(sceneInfo.scene) {}
        SceneInfo(Priority priority, const std::shared_ptr<sp::Scene> &scene) : priority(priority), scene(scene) {}

        inline bool operator==(const SceneInfo &other) const {
            return scene == other.scene;
        }

        // Should be called on the live SceneInfo
        void InsertWithPriority(Lock<Write<SceneInfo>> staging, const SceneInfo &stagingInfo);

        // Should be called on the live SceneInfo
        // Returns true if live SceneInfo should be removed
        bool Remove(Lock<Write<SceneInfo>> staging, const Tecs::Entity &stagingId);

        Tecs::Entity liveId, stagingId, nextStagingId;
        Priority priority = Priority::Scene;
        std::shared_ptr<sp::Scene> scene;
    };
} // namespace ecs
