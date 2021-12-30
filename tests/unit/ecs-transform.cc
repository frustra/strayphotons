#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <tests.hh>

namespace EcsTransformTests {
    using namespace testing;

    ecs::ECS World;

    void TestTransform() {
        Tecs::Entity root, a, b, c;
        {
            Timer t("Create a tree of transform parents");
            auto lock = World.StartTransaction<ecs::AddRemove>();

            root = lock.NewEntity();
            root.Set<ecs::Transform>(lock, glm::vec3(1, 2, 3));

            a = lock.NewEntity();
            auto &transformA = a.Set<ecs::Transform>(lock, glm::vec3(4, 0, 0));
            transformA.SetParent(root);

            b = lock.NewEntity();
            auto &transformB = b.Set<ecs::Transform>(lock, glm::vec3(0, 5, 0));
            transformB.SetParent(a);

            c = lock.NewEntity();
            auto &transformC = c.Set<ecs::Transform>(lock, glm::vec3(0, 0, 6));
            transformC.SetParent(a);
        }
        {
            Timer t("Try reading transform positions");
            auto lock = World.StartTransaction<ecs::Read<ecs::Transform>>();

            auto &transformRoot = root.Get<ecs::Transform>(lock);
            auto &transformA = a.Get<ecs::Transform>(lock);
            auto &transformB = b.Get<ecs::Transform>(lock);
            auto &transformC = c.Get<ecs::Transform>(lock);

            AssertEqual(transformRoot.GetGlobalPosition(lock),
                glm::vec3(1, 2, 3),
                "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalPosition(lock), glm::vec3(5, 2, 3), "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalPosition(lock), glm::vec3(5, 7, 3), "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalPosition(lock), glm::vec3(5, 2, 9), "C entity returned wrong position");

            {
                Timer t2("Benchmark GetGlobalTransform() with parent transform");
                for (int i = 0; i < 100000; i++) {
                    transformC.GetGlobalTransform(lock);
                }
            }
        }
        {
            Timer t("Try updating root transform");
            auto lock = World.StartTransaction<ecs::Write<ecs::Transform>>();

            auto &transformRoot = root.Get<ecs::Transform>(lock);
            auto &transformA = a.Get<ecs::Transform>(lock);
            auto &transformB = b.Get<ecs::Transform>(lock);
            auto &transformC = c.Get<ecs::Transform>(lock);

            transformRoot.SetPosition(glm::vec3(-1, -2, -3));

            AssertEqual(transformRoot.GetGlobalPosition(lock),
                glm::vec3(-1, -2, -3),
                "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalPosition(lock), glm::vec3(3, -2, -3), "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalPosition(lock), glm::vec3(3, 3, -3), "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalPosition(lock), glm::vec3(3, -2, 3), "C entity returned wrong position");
        }
        {
            Timer t("Try setting and reading rotation + scale");
            auto lock = World.StartTransaction<ecs::Write<ecs::Transform>>();

            glm::quat rotation1 = glm::rotate(glm::identity<glm::quat>(), 5.f, glm::vec3(1, 0, 0));
            glm::quat rotation2 = glm::rotate(glm::identity<glm::quat>(), 8.f, glm::normalize(glm::vec3(0, 1, 1)));
            glm::quat rotation3 = rotation1 * rotation2;
            ecs::Transform transform(glm::vec3(4, 5, 6), rotation1);

            AssertEqual(transform.GetRotation(), rotation1, "Expected rotation to be initilized");
            transform.SetScale(glm::vec3(1, 2, 3));
            AssertEqual(transform.GetRotation(), rotation1, "Expected rotation to be unchanged");
            transform.Rotate(8, glm::normalize(glm::vec3(0, 1, 1)));
            AssertEqual(transform.GetScale(), glm::vec3(1, 2, 3), "Expected scale to be unchanged");
            AssertEqual(transform.GetRotation(), rotation3, "Expected rotation to add up correctly");
            transform.SetRotation(rotation1);
            AssertEqual(transform.GetScale(), glm::vec3(1, 2, 3), "Expected scale to be unchanged");
            AssertEqual(transform.GetRotation(), rotation1, "Expected setting rotation to readback correctly");

            AssertEqual(transform.GetPosition(), glm::vec3(4, 5, 6), "Expected position to be unchanged");
        }
    }

    Test test1(&TestTransform);
} // namespace EcsTransformTests
