#include "SceneInfo.hh"

#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    void SceneInfo::InsertWithPriority(Lock<Write<SceneInfo>> staging, const SceneInfo &newSceneInfo) {
        Assert(this->liveId, "InsertWithPriority called on an invalid SceneInfo");
        Assert(this->rootStagingId.Has<SceneInfo>(staging), "InsertWithPriority called on an invalid SceneInfo");
        Assert(newSceneInfo.rootStagingId.Has<SceneInfo>(staging),
            "InsertWithPriority called with an invalid new SceneInfo");

        auto &newStagingInfo = newSceneInfo.rootStagingId.Get<SceneInfo>(staging);
        auto lastStagingInfo = &newStagingInfo;
        while (lastStagingInfo->nextStagingId.Has<SceneInfo>(staging)) {
            auto &sceneInfo = lastStagingInfo->nextStagingId.Get<SceneInfo>(staging);
            Assert(sceneInfo.priority == newSceneInfo.priority,
                "SceneInfo::InsertWithPriority input entities must all have same priority");
            lastStagingInfo = &sceneInfo;
        }

        if (newSceneInfo.priority > this->priority) {
            // Insert into the root of the linked-list
            lastStagingInfo->nextStagingId = this->rootStagingId;
            this->rootStagingId = newSceneInfo.rootStagingId;
            this->nextStagingId = newSceneInfo.nextStagingId;
            this->priority = newSceneInfo.priority;
            this->scene = newSceneInfo.scene;
            if (newSceneInfo.properties) this->properties = newSceneInfo.properties;
        } else {
            // Search the linked-list for a place to insert
            auto &rootStagingInfo = this->rootStagingId.Get<SceneInfo>(staging);
            bool propertiesSet = false;
            SceneInfo *prevSceneInfo = &rootStagingInfo;
            auto nextId = rootStagingInfo.nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                if (prevSceneInfo->properties) propertiesSet = true;

                auto &nextSceneInfo = nextId.Get<SceneInfo>(staging);
                if (newSceneInfo.priority > nextSceneInfo.priority) break;
                nextId = nextSceneInfo.nextStagingId;
                prevSceneInfo = &nextSceneInfo;
            }
            if (prevSceneInfo->nextStagingId == newSceneInfo.rootStagingId) {
                // SceneInfo is already inserted
                return;
            }

            if (!propertiesSet && newSceneInfo.properties) this->properties = newSceneInfo.properties;
            lastStagingInfo->nextStagingId = prevSceneInfo->nextStagingId;
            prevSceneInfo->nextStagingId = newSceneInfo.rootStagingId;
            this->nextStagingId = rootStagingInfo.nextStagingId;
        }
    }

    // Should be called on the live SceneInfo
    // Returns true if live SceneInfo should be removed
    bool SceneInfo::Remove(Lock<Write<SceneInfo>> staging, const Entity &removeId) {
        Assert(this->liveId, "Remove called on an invalid SceneInfo");
        Assert(this->rootStagingId.Has<SceneInfo>(staging), "Remove called on an invalid SceneInfo");
        auto &stagingInfo = this->rootStagingId.Get<const SceneInfo>(staging);

        const SceneInfo *removedEntry = nullptr;
        if (this->rootStagingId == removeId) {
            // Remove the linked-list root node
            this->rootStagingId = this->nextStagingId;
            if (this->nextStagingId.Has<SceneInfo>(staging)) {
                auto &nextStagingInfo = this->nextStagingId.Get<SceneInfo>(staging);
                nextStagingInfo.rootStagingId = this->rootStagingId;
                this->nextStagingId = nextStagingInfo.nextStagingId;
                this->priority = nextStagingInfo.priority;
                this->scene = nextStagingInfo.scene;

                auto nextId = nextStagingInfo.nextStagingId;
                while (nextId.Has<ecs::SceneInfo>(staging)) {
                    auto &nextSceneInfo = nextId.Get<ecs::SceneInfo>(staging);
                    nextSceneInfo.rootStagingId = this->rootStagingId;
                    nextId = nextSceneInfo.nextStagingId;
                }
            }
            removedEntry = &stagingInfo;
        } else if (this->nextStagingId.Has<SceneInfo>(staging)) {
            // Search the linked-list for the id to remove
            SceneInfo *prevSceneInfo = &this->rootStagingId.Get<SceneInfo>(staging);
            auto nextId = this->nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                auto &sceneInfo = nextId.Get<SceneInfo>(staging);
                if (nextId == removeId) {
                    prevSceneInfo->nextStagingId = sceneInfo.nextStagingId;
                    removedEntry = &sceneInfo;
                    break;
                }
                nextId = sceneInfo.nextStagingId;
                prevSceneInfo = &sceneInfo;
            }
            this->nextStagingId = stagingInfo.nextStagingId;
            Assertf(removedEntry, "Expected to find removal id %s in SceneInfo tree", std::to_string(removeId));
        }
        if (!this->rootStagingId) return true;

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
