#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <tests.hh>

namespace SceneManagerTests {
    using namespace testing;

    sp::SceneManager &Scenes() {
        static sp::SceneManager scenes(true);
        return scenes;
    }

    template<typename T>
    ecs::Entity EntityWith(ecs::Lock<ecs::Read<T>> lock, const T &value) {
        for (auto e : lock.template EntitiesWith<T>()) {
            if (e.template Has<T>(lock) && e.template Get<const T>(lock) == value) return e;
        }
        return {};
    }

    void AssertEntityScene(ecs::Lock<ecs::Read<ecs::Name, ecs::SceneInfo>> stagingLock,
        ecs::Lock<ecs::Read<ecs::Name, ecs::SceneInfo>> liveLock,
        std::string sceneName,
        std::string entityName,
        std::initializer_list<std::string> sceneNames) {
        AssertTrue(sceneNames.size() > 0, "AssertEntityScene expects at least 1 scene name");

        auto liveEnt = EntityWith<ecs::Name>(liveLock, ecs::Name(sceneName, entityName));
        Assertf(!!liveEnt, "Expected entity to exist: %s", entityName);
        Assertf(liveEnt.Has<ecs::SceneInfo>(liveLock), "Expected entity %s to have SceneInfo", entityName);
        auto &liveSceneInfo = liveEnt.Get<ecs::SceneInfo>(liveLock);
        AssertEqual(liveSceneInfo.liveId, liveEnt, "Staging SceneInfo.liveId does not match");

        auto ent = liveSceneInfo.rootStagingId;
        Assertf(!!ent, "Expected entity to exist: %s", std::to_string(ent));
        Assertf(ent.Has<ecs::SceneInfo>(stagingLock), "Expected %s to have SceneInfo", std::to_string(ent));
        auto &rootSceneInfo = ent.Get<ecs::SceneInfo>(stagingLock);
        AssertEqual(liveSceneInfo.nextStagingId,
            rootSceneInfo.nextStagingId,
            "Live SceneInfo.nextStagingId does not match");
        AssertTrue(!!rootSceneInfo.scene, "Expected entity to have valid Scene");
        AssertEqual(rootSceneInfo.scene.data->name, *sceneNames.begin(), "Entity scene does not match expected");

        for (auto &name : sceneNames) {
            Assertf(!!ent, "Expected entity to exist: %s", std::to_string(ent));
            Assertf(ent.Has<ecs::SceneInfo>(stagingLock), "Expected %s to have SceneInfo", std::to_string(ent));
            auto &sceneInfo = ent.Get<ecs::SceneInfo>(stagingLock);
            AssertEqual(sceneInfo.liveId, liveEnt, "Staging SceneInfo.liveId does not match");
            AssertEqual(sceneInfo.rootStagingId,
                rootSceneInfo.rootStagingId,
                "Staging SceneInfo.rootStagingId does not match");
            AssertTrue(sceneInfo.scene, "Expected entity to have valid Scene");
            AssertEqual(sceneInfo.scene.data->name, name, "Entity scene does not match expected");
            ent = sceneInfo.nextStagingId;
        }
        AssertTrue(!ent, "Expected no more entity scenes");
    }

    void systemSceneCallback(ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<sp::Scene> scene) {
        auto ent = lock.NewEntity();
        ent.Set<ecs::Name>(lock, "player", "player");
        ent.Set<ecs::SceneInfo>(lock, ent, scene);
        ent.Set<ecs::TransformSnapshot>(lock, glm::vec3(1, 2, 3));
        ent.Set<ecs::SignalOutput>(lock);
        ent.Set<ecs::SignalBindings>(lock);
        ent.Set<ecs::EventInput>(lock);
        ent.Set<ecs::EventBindings>(lock);
        ent.Set<ecs::Scripts>(lock);

        ent = lock.NewEntity();
        ent.Set<ecs::Name>(lock, "", "test");
        ent.Set<ecs::SceneInfo>(lock, ent, scene);
    }

    void TestBasicLoadAddRemove() {
        {
            Timer t("Add system scene first");
            Scenes().QueueActionAndBlock(sp::SceneAction::ApplySystemScene, "system", systemSceneCallback);

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"system"});
            }
        }
        {
            Timer t("Add player scene second");
            Scenes().QueueActionAndBlock(sp::SceneAction::ReloadPlayer);

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"player", "system"});
            }
        }
        {
            Timer t("Unload player scene (primary player entity)");
            Scenes().QueueActionAndBlock([] {
                auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

                auto player = EntityWith<ecs::Name>(liveLock, ecs::Name("player", "player"));
                AssertTrue(player.Has<ecs::Name, ecs::SceneInfo>(liveLock), "Expected player entity to be valid");
                AssertEqual(player.Get<ecs::Name>(liveLock),
                    ecs::Name("player", "player"),
                    "Expected player to be named correctly");
                auto &playerSceneInfo = player.Get<ecs::SceneInfo>(liveLock);
                auto playerScene = playerSceneInfo.scene.Lock();
                AssertTrue(!!playerScene, "Expected player to have a scene");
                AssertTrue(!!playerScene->data, "Expected player scene to have valid data");
                AssertEqual(playerScene->data->name, "player", "Expected player scene to be named correctly");
                playerScene->RemoveScene(stagingLock, liveLock);
            });

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"system"});
            }
        }
        {
            Timer t("Reload player scene");
            Scenes().QueueActionAndBlock(sp::SceneAction::ReloadPlayer);

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"player", "system"});
            }
        }
        {
            Timer t("Reset ECS");
            Scenes().QueueActionAndBlock(sp::SceneAction::RemoveScene, "system");

            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
            auto liveLock = ecs::StartTransaction<ecs::AddRemove>();

            for (auto &e : stagingLock.Entities()) {
                e.Destroy(stagingLock);
            }
            for (auto &e : liveLock.Entities()) {
                e.Destroy(liveLock);
            }
        }
        {
            Timer t("Add player scene first");
            Scenes().QueueActionAndBlock(sp::SceneAction::ReloadPlayer);

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"player"});
            }
        }
        {
            Timer t("Add system scene second");
            Scenes().QueueActionAndBlock(sp::SceneAction::ApplySystemScene, "system", systemSceneCallback);

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"player", "system"});
            }
        }
        {
            Timer t("Unload system scene (secondary player entity)");
            Scenes().QueueActionAndBlock(sp::SceneAction::RemoveScene, "system");

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"player"});
            }
        }
        {
            Timer t("Reload system scene");
            Scenes().QueueActionAndBlock(sp::SceneAction::ApplySystemScene, "system", systemSceneCallback);

            {
                auto stagingLock = ecs::StartStagingTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", "player", {"player", "system"});
            }
        }
    }

    Test test(&TestBasicLoadAddRemove);
} // namespace SceneManagerTests
