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

        auto ent = liveSceneInfo.stagingId;
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

    void TestBasicLoadAddRemove() {
        {
            Timer t("Add test system scene");
            Scenes.AddToSystemScene([](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<sp::Scene> scene) {
                auto ent = lock.NewEntity();
                ent.Set<ecs::Name>(lock, "vr-origin");
                ent.Set<ecs::SceneInfo>(lock, ent, scene);
                ent.Set<ecs::Transform>(lock, glm::vec3(1, 2, 3));

                ent = lock.NewEntity();
                ent.Set<ecs::Name>(lock, "console-input");
                ent.Set<ecs::SceneInfo>(lock, ent, scene);
                ent.Set<ecs::EventInput>(lock);
                ent.Set<ecs::Script>(lock);
            });

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "vr-origin", {"system"});
                AssertEntityScene(stagingLock, liveLock, "console-input", {"system"});
            }
        }
        {
            Timer t("Add player scene");
            Scenes.LoadPlayer();

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "vr-origin", {"player", "system"});
                AssertEntityScene(stagingLock, liveLock, "console-input", {"system"});

                AssertEntityScene(stagingLock, liveLock, "player", {"player"});
                AssertEntityScene(stagingLock, liveLock, "left-hand-input", {"player"});
                AssertEntityScene(stagingLock, liveLock, "right-hand-input", {"player"});
            }
        }
        {
            Timer t("Add bindings scene");
            Scenes.LoadBindings();

            {
                auto stagingLock = stagingWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                auto liveLock = liveWorld.StartTransaction<ecs::Read<ecs::Name, ecs::SceneInfo>>();
                AssertEntityScene(stagingLock, liveLock, "vr-origin", {"player", "system"});
                AssertEntityScene(stagingLock, liveLock, "console-input", {"system"});

                AssertEntityScene(stagingLock, liveLock, "player", {"bindings", "player"});
                AssertEntityScene(stagingLock, liveLock, "left-hand-input", {"player"});
                AssertEntityScene(stagingLock, liveLock, "right-hand-input", {"player"});

                AssertEntityScene(stagingLock, liveLock, "keyboard", {"bindings"});
                AssertEntityScene(stagingLock, liveLock, "mouse", {"bindings"});
                AssertEntityScene(stagingLock, liveLock, "vr-controller-left", {"bindings"});
                AssertEntityScene(stagingLock, liveLock, "vr-controller-right", {"bindings"});
            }
        }
    }

    Test test(&TestBasicLoadAddRemove);
} // namespace SceneManagerTests
