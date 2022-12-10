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
        auto lastStagingInfo = &newStagingInfo;
        while (lastStagingInfo->nextStagingId.Has<SceneInfo>(staging)) {
            lastStagingInfo = &lastStagingInfo->nextStagingId.Get<SceneInfo>(staging);
        }

        if (newSceneInfo.priority < this->priority) {
            // Insert into the root of the linked-list
            lastStagingInfo->nextStagingId = this->stagingId;
            this->nextStagingId = newStagingInfo.nextStagingId;
            this->stagingId = newSceneInfo.stagingId;
            this->priority = newSceneInfo.priority;
            this->scene = newSceneInfo.scene;
            if (newSceneInfo.properties) this->properties = newSceneInfo.properties;
        } else {
            // Search the linked-list for a place to insert
            auto &rootStagingInfo = this->stagingId.Get<SceneInfo>(staging);
            bool propertiesSet = false;
            SceneInfo *prevSceneInfo = &rootStagingInfo;
            auto nextId = rootStagingInfo.nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                if (prevSceneInfo->properties) propertiesSet = true;
                auto &nextSceneInfo = nextId.Get<SceneInfo>(staging);
                if (newSceneInfo.priority < nextSceneInfo.priority) break;
                nextId = nextSceneInfo.nextStagingId;
                prevSceneInfo = &nextSceneInfo;
            }
            if (nextId == newSceneInfo.stagingId) {
                // SceneInfo is already inserted
                return;
            }
            if (!propertiesSet && newSceneInfo.properties) this->properties = newSceneInfo.properties;
            lastStagingInfo->nextStagingId = nextId;
            prevSceneInfo->nextStagingId = newSceneInfo.stagingId;
            this->nextStagingId = rootStagingInfo.nextStagingId;
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
                nextStagingInfo.stagingId = this->nextStagingId;
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

        auto nextId = removedEntry->nextStagingId;
        while (nextId.Has<ecs::SceneInfo>(staging)) {
            auto &nextSceneInfo = nextId.Get<ecs::SceneInfo>(staging);
            if (nextSceneInfo.stagingId != removeId) break;

            nextSceneInfo.stagingId = removedEntry->nextStagingId;
            nextId = nextSceneInfo.nextStagingId;
        }
        if (!this->stagingId) return true;

        if (this->properties && this->properties == removedEntry->properties) {
            this->properties.reset();
            // Find next highest priority scene properties
            nextId = removedEntry->nextStagingId;
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

    const Transform &SceneInfo::GetRootTransform(Lock<Read<SceneInfo>> lock, Entity ent) {
        static Transform identity = {};
        if (ent.Has<SceneInfo>(lock)) {
            auto &sceneInfo = ent.Get<SceneInfo>(lock);
            auto scene = sceneInfo.scene.lock();
            if (scene) return scene->GetRootTransform();
        }
        return identity;
    }
} // namespace ecs
