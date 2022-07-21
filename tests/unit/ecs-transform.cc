#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <tests.hh>

namespace EcsTransformTests {
    using namespace testing;

    void TestTransformTree() {
        Tecs::Entity root, a, b, c;
        {
            Timer t("Create a tree of transform parents");
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            root = lock.NewEntity();
            ecs::EntityRef rootRef(ecs::Name("", "root"), root);
            root.Set<ecs::TransformTree>(lock, glm::vec3(1, 2, 3));

            a = lock.NewEntity();
            ecs::EntityRef aRef(ecs::Name("", "a"), a);
            auto &transformA = a.Set<ecs::TransformTree>(lock, glm::vec3(4, 0, 0));
            transformA.parent = root;

            b = lock.NewEntity();
            auto &transformB = b.Set<ecs::TransformTree>(lock, glm::vec3(0, 5, 0));
            transformB.parent = a;

            c = lock.NewEntity();
            auto &transformC = c.Set<ecs::TransformTree>(lock, glm::vec3(0, 0, 6));
            transformC.parent = a;
        }
        {
            Timer t("Try reading transform positions");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::TransformTree>>();

            auto &transformRoot = root.Get<ecs::TransformTree>(lock);
            auto &transformA = a.Get<ecs::TransformTree>(lock);
            auto &transformB = b.Get<ecs::TransformTree>(lock);
            auto &transformC = c.Get<ecs::TransformTree>(lock);

            AssertEqual(transformRoot.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(1, 2, 3),
                "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(5, 2, 3),
                "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(5, 7, 3),
                "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(5, 2, 9),
                "C entity returned wrong position");

            {
                Timer t2("Benchmark GetGlobalTransform() with parent transform");
                for (int i = 0; i < 100000; i++) {
                    transformC.GetGlobalTransform(lock);
                }
            }
        }
        {
            Timer t("Try updating root transform");
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::TransformTree>>();

            auto &transformRoot = root.Get<ecs::TransformTree>(lock);
            auto &transformA = a.Get<ecs::TransformTree>(lock);
            auto &transformB = b.Get<ecs::TransformTree>(lock);
            auto &transformC = c.Get<ecs::TransformTree>(lock);

            transformRoot.pose.SetPosition(glm::vec3(-1, -2, -3));

            AssertEqual(transformRoot.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(-1, -2, -3),
                "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(3, -2, -3),
                "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(3, 3, -3),
                "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalTransform(lock).GetPosition(),
                glm::vec3(3, -2, 3),
                "C entity returned wrong position");
        }
        {
            Timer t("Try setting and reading rotation + scale");
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::TransformTree>>();

            glm::quat rotation1 = glm::rotate(glm::identity<glm::quat>(), 5.f, glm::vec3(1, 0, 0));
            glm::quat rotation2 = glm::rotate(glm::identity<glm::quat>(), 8.f, glm::normalize(glm::vec3(0, 1, 1)));
            glm::quat rotation3 = rotation1 * rotation2;
            ecs::TransformTree transform(glm::vec3(4, 5, 6), rotation1);

            AssertEqual(transform.pose.GetRotation(), rotation1, "Expected rotation to be initilized");
            transform.pose.SetScale(glm::vec3(1, 2, 3));
            AssertEqual(transform.pose.GetRotation(), rotation1, "Expected rotation to be unchanged");
            transform.pose.Rotate(8, glm::normalize(glm::vec3(0, 1, 1)));
            AssertEqual(transform.pose.GetScale(), glm::vec3(1, 2, 3), "Expected scale to be unchanged");
            AssertEqual(transform.pose.GetRotation(), rotation3, "Expected rotation to add up correctly");
            transform.pose.SetRotation(rotation1);
            AssertEqual(transform.pose.GetScale(), glm::vec3(1, 2, 3), "Expected scale to be unchanged");
            AssertEqual(transform.pose.GetRotation(), rotation1, "Expected setting rotation to readback correctly");

            AssertEqual(transform.pose.GetPosition(), glm::vec3(4, 5, 6), "Expected position to be unchanged");
        }
    }

    Test test1(&TestTransformTree);
} // namespace EcsTransformTests
