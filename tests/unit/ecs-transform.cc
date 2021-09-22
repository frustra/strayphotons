#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace EcsTransformTests {
    using namespace testing;

    ecs::ECS World;

    void TestTransformParentDepth() {
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
            Timer t("Try reading transform locations with out of date caches");
            auto lock = World.StartTransaction<ecs::Read<ecs::Transform>>();

            auto &transformRoot = root.Get<ecs::Transform>(lock);
            auto &transformA = a.Get<ecs::Transform>(lock);
            auto &transformB = b.Get<ecs::Transform>(lock);
            auto &transformC = c.Get<ecs::Transform>(lock);

            Assert(!transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be out of date");
            Assert(!transformA.IsCacheUpToDate(lock), "Expected A cache to be out of date");
            Assert(!transformB.IsCacheUpToDate(lock), "Expected B cache to be out of date");
            Assert(!transformC.IsCacheUpToDate(lock), "Expected C cache to be out of date");

            AssertEqual(transformRoot.GetGlobalPosition(lock),
                        glm::vec3(1, 2, 3),
                        "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalPosition(lock), glm::vec3(5, 2, 3), "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalPosition(lock), glm::vec3(5, 7, 3), "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalPosition(lock), glm::vec3(5, 2, 9), "C entity returned wrong position");
        }
        {
            Timer t("Try updating transform caches");
            auto lock = World.StartTransaction<ecs::Write<ecs::Transform>>();

            auto &transformRoot = root.Get<ecs::Transform>(lock);
            auto &transformA = a.Get<ecs::Transform>(lock);
            auto &transformB = b.Get<ecs::Transform>(lock);
            auto &transformC = c.Get<ecs::Transform>(lock);

            Assert(!transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be out of date");
            Assert(!transformA.IsCacheUpToDate(lock), "Expected A cache to be out of date");
            Assert(!transformB.IsCacheUpToDate(lock), "Expected B cache to be out of date");
            Assert(!transformC.IsCacheUpToDate(lock), "Expected C cache to be out of date");

            transformB.UpdateCachedTransform(lock);

            Assert(transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be up to date");
            Assert(transformA.IsCacheUpToDate(lock), "Expected A cache to be up to date");
            Assert(transformB.IsCacheUpToDate(lock), "Expected B cache to be up to date");
            Assert(!transformC.IsCacheUpToDate(lock), "Expected C cache to be out of date");

            transformC.UpdateCachedTransform(lock);

            Assert(transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be up to date");
            Assert(transformA.IsCacheUpToDate(lock), "Expected A cache to be up to date");
            Assert(transformB.IsCacheUpToDate(lock), "Expected B cache to be up to date");
            Assert(transformC.IsCacheUpToDate(lock), "Expected C cache to be up to date");
        }
        {
            Timer t("Try reading transform locations with up to date caches");
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
        }
        {
            Timer t("Try updating root transform");
            auto lock = World.StartTransaction<ecs::Write<ecs::Transform>>();

            auto &transformRoot = root.Get<ecs::Transform>(lock);
            auto &transformA = a.Get<ecs::Transform>(lock);
            auto &transformB = b.Get<ecs::Transform>(lock);
            auto &transformC = c.Get<ecs::Transform>(lock);

            Assert(transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be up to date");
            Assert(transformA.IsCacheUpToDate(lock), "Expected A cache to be up to date");
            Assert(transformB.IsCacheUpToDate(lock), "Expected B cache to be up to date");
            Assert(transformC.IsCacheUpToDate(lock), "Expected C cache to be up to date");

            transformRoot.SetPosition(glm::vec3(-1, -2, -3));

            Assert(!transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be out of date");
            Assert(!transformA.IsCacheUpToDate(lock), "Expected A cache to be out of date");
            Assert(!transformB.IsCacheUpToDate(lock), "Expected B cache to be out of date");
            Assert(!transformC.IsCacheUpToDate(lock), "Expected C cache to be out of date");

            AssertEqual(transformRoot.GetGlobalPosition(lock),
                        glm::vec3(-1, -2, -3),
                        "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalPosition(lock), glm::vec3(3, -2, -3), "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalPosition(lock), glm::vec3(3, 3, -3), "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalPosition(lock), glm::vec3(3, -2, 3), "C entity returned wrong position");

            transformB.UpdateCachedTransform(lock);

            Assert(transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be up to date");
            Assert(transformA.IsCacheUpToDate(lock), "Expected A cache to be up to date");
            Assert(transformB.IsCacheUpToDate(lock), "Expected B cache to be up to date");
            Assert(!transformC.IsCacheUpToDate(lock), "Expected C cache to be out of date");

            AssertEqual(transformRoot.GetGlobalPosition(lock),
                        glm::vec3(-1, -2, -3),
                        "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalPosition(lock), glm::vec3(3, -2, -3), "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalPosition(lock), glm::vec3(3, 3, -3), "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalPosition(lock), glm::vec3(3, -2, 3), "C entity returned wrong position");

            transformC.UpdateCachedTransform(lock);

            Assert(transformRoot.IsCacheUpToDate(lock), "Expected Root cache to be up to date");
            Assert(transformA.IsCacheUpToDate(lock), "Expected A cache to be up to date");
            Assert(transformB.IsCacheUpToDate(lock), "Expected B cache to be up to date");
            Assert(transformC.IsCacheUpToDate(lock), "Expected C cache to be up to date");

            AssertEqual(transformRoot.GetGlobalPosition(lock),
                        glm::vec3(-1, -2, -3),
                        "Root entity returned wrong position");
            AssertEqual(transformA.GetGlobalPosition(lock), glm::vec3(3, -2, -3), "A entity returned wrong position");
            AssertEqual(transformB.GetGlobalPosition(lock), glm::vec3(3, 3, -3), "B entity returned wrong position");
            AssertEqual(transformC.GetGlobalPosition(lock), glm::vec3(3, -2, 3), "C entity returned wrong position");
        }
    }

    Test test(&TestTransformParentDepth);
} // namespace EcsTransformTests
