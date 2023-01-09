#include "SceneInfo.hh"

#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    SceneInfo::SceneInfo(Entity ent, const std::shared_ptr<sp::Scene> &scene)
        : priority(scene->data->priority), scene(scene) {
        if (IsLive(ent)) {
            liveId = ent;
        } else if (IsStaging(ent)) {
            rootStagingId = ent;
        } else {
            Abortf("Invalid SceneInfo entity: %s", std::to_string(ent));
        }
    }

    void SceneInfo::InsertWithPriority(Lock<Write<SceneInfo>> staging, const SceneInfo &newSceneInfo) const {
        Assert(this->rootStagingId.Has<SceneInfo>(staging), "InsertWithPriority called on an invalid SceneInfo");
        Assert(newSceneInfo.rootStagingId.Has<SceneInfo>(staging),
            "InsertWithPriority called with an invalid new SceneInfo");
        Assert(newSceneInfo.rootStagingId != this->rootStagingId,
            "InsertWithPriority called with same scene info list");

        auto &rootStagingInfo = this->rootStagingId.Get<SceneInfo>(staging);
        auto &newStagingInfo = newSceneInfo.rootStagingId.Get<SceneInfo>(staging);

        auto lastStagingInfo = &newStagingInfo;
        while (lastStagingInfo->nextStagingId.Has<SceneInfo>(staging)) {
            auto &sceneInfo = lastStagingInfo->nextStagingId.Get<SceneInfo>(staging);
            Assert(sceneInfo.priority == newSceneInfo.priority,
                "SceneInfo::InsertWithPriority input entities must all have same priority");
            lastStagingInfo = &sceneInfo;
        }

        if (newSceneInfo.priority > rootStagingInfo.priority) {
            // Insert into the root of the linked-list
            lastStagingInfo->nextStagingId = this->rootStagingId;

            // Update the root staging id in each entry
            auto nextId = lastStagingInfo->nextStagingId;
            while (nextId.Has<ecs::SceneInfo>(staging)) {
                auto &nextSceneInfo = nextId.Get<ecs::SceneInfo>(staging);
                nextSceneInfo.rootStagingId = newStagingInfo.rootStagingId;
                nextId = nextSceneInfo.nextStagingId;
            }
        } else {
            // Search the linked-list for a place to insert
            SceneInfo *prevSceneInfo = &rootStagingInfo;
            auto nextId = rootStagingInfo.nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                auto &nextSceneInfo = nextId.Get<SceneInfo>(staging);
                if (newSceneInfo.priority > nextSceneInfo.priority) break;
                nextId = nextSceneInfo.nextStagingId;
                prevSceneInfo = &nextSceneInfo;
            }
            auto tmpId = prevSceneInfo->nextStagingId;
            prevSceneInfo->nextStagingId = newSceneInfo.rootStagingId;

            // Update the root staging id in the new entries
            nextId = newSceneInfo.rootStagingId;
            while (nextId.Has<ecs::SceneInfo>(staging)) {
                auto &nextSceneInfo = nextId.Get<ecs::SceneInfo>(staging);
                nextSceneInfo.rootStagingId = this->rootStagingId;
                nextId = nextSceneInfo.nextStagingId;
            }
            lastStagingInfo->nextStagingId = tmpId;
        }
    }

    void SceneInfo::SetLiveId(Lock<Write<SceneInfo>> staging, Entity liveId) const {
        Assert(IsStaging(staging), "SceneInfo::SetLiveId() must be called with staging lock");
        Assert(!liveId || IsLive(liveId), "SceneInfo::SetLiveId() must be called with a live entity");
        Assert(this->rootStagingId.Has<SceneInfo>(staging), "SceneInfo::SetLiveId called on an invalid SceneInfo");

        auto nextId = this->rootStagingId;
        while (nextId.Has<ecs::SceneInfo>(staging)) {
            auto &sceneInfo = nextId.Get<ecs::SceneInfo>(staging);
            sceneInfo.liveId = liveId;
            nextId = sceneInfo.nextStagingId;
        }
    }

    Entity SceneInfo::Remove(Lock<Write<SceneInfo>> staging, const Entity &removeId) const {
        Assert(this->rootStagingId.Has<SceneInfo>(staging), "Remove called on an invalid SceneInfo");
        auto &stagingInfo = this->rootStagingId.Get<SceneInfo>(staging);

        Entity remainingId;
        if (stagingInfo.rootStagingId == removeId) {
            remainingId = stagingInfo.nextStagingId;

            // Remove the linked-list root node
            auto nextId = stagingInfo.nextStagingId;
            while (nextId.Has<ecs::SceneInfo>(staging)) {
                auto &nextSceneInfo = nextId.Get<ecs::SceneInfo>(staging);
                nextSceneInfo.rootStagingId = stagingInfo.nextStagingId;
                nextId = nextSceneInfo.nextStagingId;
            }
            stagingInfo.nextStagingId = {};
        } else if (stagingInfo.nextStagingId.Has<SceneInfo>(staging)) {
            remainingId = stagingInfo.rootStagingId; // Linked list root stays the same

            // Search the linked-list for the id to remove
            SceneInfo *prevSceneInfo = &stagingInfo;
            auto nextId = prevSceneInfo->nextStagingId;
            while (nextId.Has<SceneInfo>(staging)) {
                auto &sceneInfo = nextId.Get<SceneInfo>(staging);
                if (nextId == removeId) {
                    prevSceneInfo->nextStagingId = sceneInfo.nextStagingId;
                    break;
                }
                nextId = sceneInfo.nextStagingId;
                prevSceneInfo = &sceneInfo;
            }
        }
        return remainingId;
    }
} // namespace ecs
