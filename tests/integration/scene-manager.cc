#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <tests.hh>

namespace SceneManagerTests {
    using namespace testing;

    ecs::ECS liveWorld;
    ecs::ECS stagingWorld;
    sp::SceneManager Scenes(liveWorld, stagingWorld);

    void AssertEntityScene(ecs::Lock<ecs::Read<ecs::Name, ecs::SceneInfo>> stagingLock,
        ecs::Lock<ecs::Read<ecs::Name, ecs::SceneInfo>> liveLock,
        std::string entityName,
        std::initializer_list<std::string> sceneNames) {
        Assert(sceneNames.size() > 0, "AssertEntityScene expects at least 1 scene name");

        auto liveEnt = ecs::EntityWith<ecs::Name>(liveLock, entityName);
        Assertf(!!liveEnt, "Expected entity to exist: %s", entityName);
        Assertf(liveEnt.Has<ecs::SceneInfo>(liveLock), "Expected entity %s to have SceneInfo", entityName);
        auto &liveSceneInfo = liveEnt.Get<ecs::SceneInfo>(liveLock);
        AssertEqual(liveSceneInfo.liveId, liveEnt, "Staging SceneInfo.liveId does not match");

        auto ent = liveSceneInfo.stagingId;
        Assertf(!!ent, "Expected entity to exist: %u", ent.id);
        Assertf(ent.Has<ecs::SceneInfo>(stagingLock), "Expected entity %u to have SceneInfo", ent.id);
        auto &sceneInfo = ent.Get<ecs::SceneInfo>(stagingLock);
        AssertEqual(liveSceneInfo.nextStagingId,
            sceneInfo.nextStagingId,
            "Live SceneInfo.nextStagingId does not match");
        Assert(sceneInfo.scene != nullptr, "Expected entity to have valid Scene");
        AssertEqual(sceneInfo.scene->sceneName, *sceneNames.begin(), "Entity scene does not match expected");

        for (auto &sceneName : sceneNames) {
            Assertf(!!ent, "Expected entity to exist: %u", ent.id);
            Assertf(ent.Has<ecs::SceneInfo>(stagingLock), "Expected entity %u to have SceneInfo", ent.id);
            auto &sceneInfo = ent.Get<ecs::SceneInfo>(stagingLock);
            AssertEqual(sceneInfo.liveId, liveEnt, "Staging SceneInfo.liveId does not match");
            AssertEqual(sceneInfo.stagingId, ent, "Staging SceneInfo.stagingId does not match");
            Assert(sceneInfo.scene != nullptr, "Expected entity to have valid Scene");
            AssertEqual(sceneInfo.scene->sceneName, sceneName, "Entity scene does not match expected");
            ent = sceneInfo.nextStagingId;
        }
        Assert(!ent, "Expected no more entity scenes");
    }

    void systemSceneCallback(ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<sp::Scene> scene) {
        auto ent = lock.NewEntity();
        ent.Set<ecs::Name>(lock, "player");
        ent.Set<ecs::SceneInfo>(lock, ent, ecs::SceneInfo::Priority::System, scene);
        ent.Set<ecs::Transform>(lock, glm::vec3(1, 2, 3));
        ent.Set<ecs::SignalOutput>(lock);
        ent.Set<ecs::SignalBindings>(lock);
        ent.Set<ecs::EventInput>(lock);
        ent.Set<ecs::EventBindings>(lock);
        ent.Set<ecs::Script>(lock);

        ent = lock.NewEntity();
        ent.Set<ecs::Name>(lock, "test");
        ent.Set<ecs::SceneInfo>(lock, ent, ecs::SceneInfo::Priority::System, scene);
    }

    void TestBasicLoadAddRemove() {
        {
            Timer t("Add system scene first");
            Scenes.AddSystemEntities(systemSceneCallback);

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"system"});
            }
        }
        {
            Timer t("Add player scene second");
            Scenes.LoadPlayer();

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"player", "system"});
            }
        }
        {
            Timer t("Unload player scene (primary player entity)");
            auto player = Scenes.GetPlayer();
            std::shared_ptr<sp::Scene> playerScene;
            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
                auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();
                Assert(player.Has<ecs::Name, ecs::SceneInfo>(liveLock), "Expected player entity to be valid");
                AssertEqual(player.Get<ecs::Name>(liveLock), "player", "Expected player to be named correctly");
                auto &playerSceneInfo = player.Get<ecs::SceneInfo>(liveLock);
                playerScene = playerSceneInfo.scene;
                Assert(playerScene != nullptr, "Expected player to have a scene");
                AssertEqual(playerScene->sceneName, "player", "Expected player scene to be named correctly");
                playerScene->RemoveScene(stagingLock, liveLock);
            }

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"system"});
            }
        }
        {
            Timer t("Reload player scene");
            Scenes.LoadPlayer();

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"player", "system"});
            }
        }
        {
            Timer t("Reset ECS");
            auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
            auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();

            for (auto &e : stagingLock.Entities()) {
                e.Destroy(stagingLock);
            }
            for (auto &e : liveLock.Entities()) {
                e.Destroy(liveLock);
            }
        }
        {
            Timer t("Add player scene first");
            Scenes.LoadPlayer();

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"player"});
            }
        }
        {
            Timer t("Add system scene second");
            Scenes.AddSystemEntities(systemSceneCallback);

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"player", "system"});
            }
        }
        {
            Timer t("Unload system scene (secondary player entity)");
            std::shared_ptr<sp::Scene> systemScene;
            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
                auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();
                auto test = ecs::EntityWith<ecs::Name>(stagingLock, "test");
                Assert(test.Has<ecs::Name, ecs::SceneInfo>(stagingLock), "Expected test entity to be valid");
                AssertEqual(test.Get<ecs::Name>(stagingLock), "test", "Expected test entity to be named correctly");
                auto &testSceneInfo = test.Get<ecs::SceneInfo>(stagingLock);
                systemScene = testSceneInfo.scene;
                Assert(systemScene != nullptr, "Expected test entity to have a scene");
                AssertEqual(systemScene->sceneName, "system", "Expected system scene to be named correctly");
                systemScene->RemoveScene(stagingLock, liveLock);
            }

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"player"});
            }
        }
        {
            Timer t("Reload system scene");
            Scenes.AddSystemEntities(systemSceneCallback);

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player", {"player", "system"});
            }
        }
    }

    Test test(&TestBasicLoadAddRemove);
} // namespace SceneManagerTests
