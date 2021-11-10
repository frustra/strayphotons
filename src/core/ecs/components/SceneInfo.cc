#include "SceneInfo.hh"

#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    // Should be called on the live SceneInfo
    void SceneInfo::InsertWithPriority(Lock<Write<SceneInfo>> staging, const SceneInfo &newSceneInfo) {
        Assert(this->liveId, "InsertWithPriority called on an invalid SceneInfo");
        Assert(this->stagingId.Has<SceneInfo>(staging), "InsertWithPriority called on an invalid SceneInfo");
        Assert(newSceneInfo.stagingId.Has<SceneInfo>(staging),
            "InsertWithPriority called with an invalid new SceneInfo");
        auto &newStagingInfo = newSceneInfo.stagingId.Get<SceneInfo>(staging);

        if (newSceneInfo.priority >= this->priority) {
            // Insert into the root of the linked-list
            this->nextStagingId = newStagingInfo.nextStagingId = this->stagingId;
            this->stagingId = newSceneInfo.stagingId;
            this->priority = newSceneInfo.priority;
            this->scene = newSceneInfo.scene;
        } else {
            // Search the linked-list for a place to insert
            auto &stagingInfo = this->stagingId.Get<SceneInfo>(staging);
            SceneInfo *prevSceneInfo = &stagingInfo;
            auto nextId = stagingInfo.nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                auto &nextSceneInfo = nextId.Get<SceneInfo>(staging);
                if (newSceneInfo.priority >= nextSceneInfo.priority) break;
                nextId = nextSceneInfo.nextStagingId;
                prevSceneInfo = &nextSceneInfo;
            }
            if (nextId == newSceneInfo.stagingId) {
                // SceneInfo is already inserted
                return;
            }
            newStagingInfo.nextStagingId = nextId;
            prevSceneInfo->nextStagingId = newSceneInfo.stagingId;
            this->nextStagingId = stagingInfo.nextStagingId;
        }
    }

    // Should be called on the live SceneInfo
    // Returns true if live SceneInfo should be removed
    bool SceneInfo::Remove(Lock<Write<SceneInfo>> staging, const Tecs::Entity &removeId) {
        Assert(this->liveId, "Remove called on an invalid SceneInfo");
        Assert(this->stagingId.Has<SceneInfo>(staging), "Remove called on an invalid SceneInfo");

        if (this->stagingId == removeId) {
            // Remove the linked-list root node
            this->stagingId = this->nextStagingId;
            if (this->stagingId.Has<SceneInfo>(staging)) {
                auto &stagingInfo = this->stagingId.Get<SceneInfo>(staging);
                this->nextStagingId = stagingInfo.nextStagingId;
                this->priority = stagingInfo.priority;
                this->scene = stagingInfo.scene;
            }
        } else if (this->nextStagingId.Has<SceneInfo>(staging)) {
            // Search the linked-list for the id to remove
            auto &stagingInfo = this->stagingId.Get<SceneInfo>(staging);
            SceneInfo *prevSceneInfo = &stagingInfo;
            auto nextId = this->nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                auto &sceneInfo = nextId.Get<SceneInfo>(staging);
                if (sceneInfo.stagingId == removeId) {
                    prevSceneInfo->nextStagingId = sceneInfo.nextStagingId;
                    break;
                }
                nextId = sceneInfo.nextStagingId;
                prevSceneInfo = &sceneInfo;
            }
            this->nextStagingId = stagingInfo.nextStagingId;
        }
        return !this->stagingId;
    }
} // namespace ecs
