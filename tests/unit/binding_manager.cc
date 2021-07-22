#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace BindingManagerTest {
    using namespace testing;

    const std::string TEST_SOURCE_BUTTON = "/device1/button";
    const std::string TEST_SOURCE_TRIGGER = "/device1/trigger";
    const std::string TEST_SOURCE_KEY = "/device2/key";
    const std::string TEST_EVENT_ACTION1 = "/test/action1";
    const std::string TEST_EVENT_ACTION2 = "/test/action2";
    const std::string TEST_SIGNAL_ACTION = "test_action";

    void TrySendEvent() {
        ecs::EntityManager ecs;

        Tecs::Entity player;
        {
            Timer t("Add player with EventInput component");
            auto lock = ecs.tecs.StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            player.Set<ecs::Name>(lock, "player");
            auto &eventInput = player.Set<ecs::EventInput>(lock, TEST_EVENT_ACTION1, TEST_EVENT_ACTION2);
            AssertEqual(eventInput.events.size(), 2u, "EventInput did not save correctly");
        }
        {
            Timer t("Send some test events");
            auto lock = ecs.tecs.StartTransaction<ecs::Write<ecs::EventInput>>();

            auto &events = player.Get<ecs::EventInput>(lock);

            ecs::Event testEvent1(TEST_SOURCE_BUTTON, player, 42);
            events.Add(TEST_EVENT_ACTION1, testEvent1);
            ecs::Event testEvent2(TEST_SOURCE_KEY, player, 'a');
            events.Add(TEST_EVENT_ACTION2, testEvent2);
            testEvent2.data = 'b';
            events.Add(TEST_EVENT_ACTION2, testEvent2);
        }
        {
            Timer t("Read the test events");
            auto lock = ecs.tecs.StartTransaction<ecs::Write<ecs::EventInput>>();

            auto &events = player.Get<ecs::EventInput>(lock);

            ecs::Event event;
            Assert(events.Poll(TEST_EVENT_ACTION1, event), "Expected to receive an event");
            AssertEqual(event.data, ecs::Event::EventData(42), "Unexpected event data");
            Assert(!events.Poll(TEST_EVENT_ACTION1, event), "Unexpected second event");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should be reset");

            Assert(events.Poll(TEST_EVENT_ACTION2, event), "Expected to receive an event");
            AssertEqual(event.data, ecs::Event::EventData('a'), "Unexpected event data");
            Assert(events.Poll(TEST_EVENT_ACTION2, event), "Expected to receive an event");
            AssertEqual(event.data, ecs::Event::EventData('b'), "Unexpected event data");
            Assert(!events.Poll(TEST_EVENT_ACTION2, event), "Unexpected second event");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should be reset");
        }
    }

    Test test(&TrySendEvent);
} // namespace BindingManagerTest
