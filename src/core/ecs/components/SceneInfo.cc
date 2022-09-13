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
            if (newSceneInfo.properties) this->properties = newSceneInfo.properties;
        } else {
            // Search the linked-list for a place to insert
            auto &stagingInfo = this->stagingId.Get<SceneInfo>(staging);
            bool propertiesSet = false;
            SceneInfo *prevSceneInfo = &stagingInfo;
            auto nextId = stagingInfo.nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                if (prevSceneInfo->properties) propertiesSet = true;
                auto &nextSceneInfo = nextId.Get<SceneInfo>(staging);
                if (newSceneInfo.priority >= nextSceneInfo.priority) break;
                nextId = nextSceneInfo.nextStagingId;
                prevSceneInfo = &nextSceneInfo;
            }
            if (nextId == newSceneInfo.stagingId) {
                // SceneInfo is already inserted
                return;
            }
            if (!propertiesSet && newSceneInfo.properties) this->properties = newSceneInfo.properties;
            newStagingInfo.nextStagingId = nextId;
            prevSceneInfo->nextStagingId = newSceneInfo.stagingId;
            this->nextStagingId = stagingInfo.nextStagingId;
        }
    }

    // Should be called on the live SceneInfo
    // Returns true if live SceneInfo should be removed
    bool SceneInfo::Remove(Lock<Write<SceneInfo>> staging, const Entity &removeId) {
        Assert(this->liveId, "Remove called on an invalid SceneInfo");
        Assert(this->stagingId.Has<SceneInfo>(staging), "Remove called on an invalid SceneInfo");
        auto &stagingInfo = this->stagingId.Get<const SceneInfo>(staging);

        const SceneInfo *removedEntry = nullptr;
        if (this->stagingId == removeId) {
            // Remove the linked-list root node
            this->stagingId = this->nextStagingId;
            if (this->nextStagingId.Has<SceneInfo>(staging)) {
                auto &nextStagingInfo = this->nextStagingId.Get<SceneInfo>(staging);
                this->nextStagingId = nextStagingInfo.nextStagingId;
                this->priority = nextStagingInfo.priority;
                this->scene = nextStagingInfo.scene;
            }
            removedEntry = &stagingInfo;
        } else if (this->nextStagingId.Has<SceneInfo>(staging)) {
            // Search the linked-list for the id to remove
            SceneInfo *prevSceneInfo = &this->stagingId.Get<SceneInfo>(staging);
            auto nextId = this->nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                auto &sceneInfo = nextId.Get<SceneInfo>(staging);
                if (sceneInfo.stagingId == removeId) {
                    prevSceneInfo->nextStagingId = sceneInfo.nextStagingId;
                    removedEntry = &sceneInfo;
                    break;
                }
                nextId = sceneInfo.nextStagingId;
                prevSceneInfo = &sceneInfo;
            }
            this->nextStagingId = stagingInfo.nextStagingId;
        }
        Assertf(removedEntry, "Expected to find removal id %s in SceneInfo tree", std::to_string(removeId));
        if (!this->stagingId) return true;

        if (this->properties && this->properties == removedEntry->properties) {
            this->properties.reset();
            // Find next highest priority scene properties
            auto nextId = removedEntry->nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                auto &nextSceneInfo = nextId.Get<SceneInfo>(staging);
                if (nextSceneInfo.properties) {
                    this->properties = nextSceneInfo.properties;
                    break;
                }
                nextId = nextSceneInfo.nextStagingId;
            }
        }
        return false;
    }
} // namespace ecs
