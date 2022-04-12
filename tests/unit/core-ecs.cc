#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace CoreEcsTests {
    using namespace testing;

    void TryAddRemove() {
        Tecs::Entity player;
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

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
            auto lock = ecs::World.StartTransaction<ecs::ReadAll>();

            auto pos = player.Get<ecs::TransformSnapshot>(lock).GetPosition();
            AssertEqual(pos, glm::vec3(1, 2, 3), "Transform did not read back correctly from new transaction");
        }
    }

    Test test(&TryAddRemove);
} // namespace CoreEcsTests
