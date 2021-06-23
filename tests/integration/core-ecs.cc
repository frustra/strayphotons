#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include "gtest/gtest.h"
#include <glm/glm.hpp>

namespace test {
    TEST(CoreEcs, TryAddRemove) {
        ASSERT_TRUE(2 == (1 + 1)) << "This is an example of a test";

        ecs::EntityManager ecs;

        Tecs::Entity player;
        {
            auto lock = ecs.tecs.StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            player.Set<ecs::Owner>(lock, ecs::Owner::SystemId::GAME_LOGIC);
            player.Set<ecs::Name>(lock, "player");
            auto &transform = player.Set<ecs::Transform>(lock, glm::vec3(1, 2, 3));
            auto pos1 = transform.GetPosition();
            ASSERT_EQ(pos1, glm::vec3(1, 2, 3)) << "Transform did not save correctly";

            auto &view = player.Set<ecs::View>(lock);
            view.clip = glm::vec2(0.1, 256);

            auto pos2 = player.Get<ecs::Transform>(lock).GetPosition();
            ASSERT_EQ(pos2, glm::vec3(1, 2, 3)) << "Transform did not read back correctly";
        }
        {
            auto lock = ecs.tecs.StartTransaction<ecs::ReadAll>();

            auto pos = player.Get<ecs::Transform>(lock).GetPosition();
            ASSERT_EQ(pos, glm::vec3(1, 2, 3)) << "Transform did not read back correctly from new transaction";
        }
    }
} // namespace test
