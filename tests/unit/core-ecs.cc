#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace CoreEcsTests {
    using namespace testing;

    void TryAddRemove() {
        Timer t("Test ecs::StartTransaction<AddRemove>");

        Tecs::Entity player;
        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            player.Set<ecs::Name>(lock, "", "player");
            auto &transform = player.Set<ecs::TransformSnapshot>(lock, glm::vec3(1, 2, 3));
            auto pos1 = transform.GetPosition();
            AssertEqual(pos1, glm::vec3(1, 2, 3), "Transform did not save correctly");

            auto &view = player.Set<ecs::View>(lock);
            view.clip = glm::vec2(0.1, 256);

            auto pos2 = player.Get<ecs::TransformSnapshot>(lock).GetPosition();
            AssertEqual(pos2, glm::vec3(1, 2, 3), "Transform did not read back correctly");
        }
        {
            auto lock = ecs::StartTransaction<ecs::ReadAll>();

            auto pos = player.Get<ecs::TransformSnapshot>(lock).GetPosition();
            AssertEqual(pos, glm::vec3(1, 2, 3), "Transform did not read back correctly from new transaction");
        }
    }

    void TryQueueTransaction() {
        Timer t("Test ecs::QueueTransaction");

        auto entFuture = ecs::QueueTransaction<ecs::AddRemove>([](auto lock) {
            return lock.NewEntity();
        });
        ecs::QueueTransaction<ecs::AddRemove>([entFuture](auto lock) {
            Assert(entFuture->Ready(), "Expected result of first transaction to be available");
            auto entPtr = entFuture->Get();
            Assert(entPtr != nullptr, "Expected future to contain a value");
            auto ent = *entPtr;
            Assert(ent.Exists(lock), "Expected entity to be available in second transaction");
            ent.Set<ecs::Name>(lock, "test", "entity");
        });
        auto result = ecs::QueueTransaction<ecs::AddRemove>([entFuture](auto lock) {
            auto ent = *entFuture->Get();
            Assert(ent.Exists(lock), "Expected entity to be available in third transaction");
            ent.Destroy(lock);
        });
        result->Get();
        Assert(entFuture->Ready(), "Expected result of first transaction to be available");

        {
            auto lock = ecs::StartTransaction<>();
            auto ent = *entFuture->Get();
            Assert((bool)ent, "Expected entity to be available in third transaction");
            Assert(!ent.Exists(lock), "Expected entity to removed after third transaction");
        }
    }

    Test test1(&TryAddRemove);
    Test test2(&TryQueueTransaction);
} // namespace CoreEcsTests
