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
        Assertf(!!ent, "Expected entity to exist: %s", std::to_string(ent));
        Assertf(ent.Has<ecs::SceneInfo>(stagingLock), "Expected %s to have SceneInfo", std::to_string(ent));
        auto &rootSceneInfo = ent.Get<ecs::SceneInfo>(stagingLock);
        AssertEqual(liveSceneInfo.nextStagingId,
            rootSceneInfo.nextStagingId,
            "Live SceneInfo.nextStagingId does not match");
        auto rootScene = rootSceneInfo.scene.lock();
        Assert(rootScene != nullptr, "Expected entity to have valid Scene");
        AssertEqual(rootScene->name, *sceneNames.begin(), "Entity scene does not match expected");

        for (auto &sceneName : sceneNames) {
            Assertf(!!ent, "Expected entity to exist: %s", std::to_string(ent));
            Assertf(ent.Has<ecs::SceneInfo>(stagingLock), "Expected %s to have SceneInfo", std::to_string(ent));
            auto &sceneInfo = ent.Get<ecs::SceneInfo>(stagingLock);
            AssertEqual(sceneInfo.liveId, liveEnt, "Staging SceneInfo.liveId does not match");
            AssertEqual(sceneInfo.stagingId, ent, "Staging SceneInfo.stagingId does not match");
            auto scene = sceneInfo.scene.lock();
            Assert(scene != nullptr, "Expected entity to have valid Scene");
            AssertEqual(scene->name, sceneName, "Entity scene does not match expected");
            ent = sceneInfo.nextStagingId;
        }
        Assert(!ent, "Expected no more entity scenes");
    }

    void systemSceneCallback(ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<sp::Scene> scene) {
        auto ent = lock.NewEntity();
        ent.Set<ecs::Name>(lock, "player.player");
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
            Scenes.AddSystemScene("system", systemSceneCallback);

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player.player", {"system"});
            }
        }
        {
            Timer t("Add player scene second");
            Scenes.LoadPlayer();

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player.player", {"player", "system"});
            }
        }
        {
            Timer t("Unload player scene (primary player entity)");
            std::shared_ptr<sp::Scene> playerScene;
            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::AddRemove>();
                auto liveLock = liveWorld.StartTransaction<ecs::AddRemove>();

                auto player = ecs::EntityWith<ecs::Name>(liveLock, "player.player");
                Assert(player.Has<ecs::Name, ecs::SceneInfo>(liveLock), "Expected player entity to be valid");
                AssertEqual(player.Get<ecs::Name>(liveLock), "player.player", "Expected player to be named correctly");
                auto &playerSceneInfo = player.Get<ecs::SceneInfo>(liveLock);
                playerScene = playerSceneInfo.scene.lock();
                Assert(playerScene != nullptr, "Expected player to have a scene");
                AssertEqual(playerScene->name, "player", "Expected player scene to be named correctly");
                playerScene->RemoveScene(stagingLock, liveLock);
            }

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player.player", {"system"});
            }
        }
        {
            Timer t("Reload player scene");
            Scenes.LoadPlayer();

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player.player", {"player", "system"});
            }
        }
        {
            Timer t("Reset ECS");
            Scenes.RemoveScene("system");

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
                AssertEntityScene(stagingLock, liveLock, "player.player", {"player"});
            }
        }
        {
            Timer t("Add system scene second");
            Scenes.AddSystemScene("system", systemSceneCallback);

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player.player", {"player", "system"});
            }
        }
        {
            Timer t("Unload system scene (secondary player entity)");
            Scenes.RemoveScene("system");

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player.player", {"player"});
            }
        }
        {
            Timer t("Reload system scene");
            Scenes.AddSystemScene("system", systemSceneCallback);

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "player.player", {"player", "system"});
            }
        }
    }

    Test test(&TestBasicLoadAddRemove);
} // namespace SceneManagerTests
