#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace EventBindingTests {
    using namespace testing;

    const std::string TEST_SOURCE_BUTTON = "/device1/button";
    const std::string TEST_SOURCE_TRIGGER = "/device1/trigger";
    const std::string TEST_SOURCE_KEY = "/device2/key";
    const std::string TEST_EVENT_ACTION1 = "/test/action1";
    const std::string TEST_EVENT_ACTION2 = "/test/action2";
    const std::string TEST_SIGNAL_ACTION = "test_action";

    void TrySendEvent() {
        ecs::EntityManager ecs;

        Tecs::Entity player, hand;
        {
            Timer t("Create a basic scene with EventBindings and EventInput components");
            auto lock = ecs.tecs.StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            player.Set<ecs::Name>(lock, "player");
            player.Set<ecs::EventInput>(lock, TEST_EVENT_ACTION2);

            hand = lock.NewEntity();
            auto &eventInput = hand.Set<ecs::EventInput>(lock, TEST_EVENT_ACTION1, TEST_EVENT_ACTION2);
            AssertEqual(eventInput.events.size(), 2u, "EventInput did not save correctly");

            auto &playerBindings = player.Set<ecs::EventBindings>(lock);
            playerBindings.Bind(TEST_SOURCE_BUTTON, hand, TEST_EVENT_ACTION1);
            playerBindings.Bind(TEST_SOURCE_KEY, hand, TEST_EVENT_ACTION2);
            playerBindings.Bind(TEST_SOURCE_KEY, player, TEST_EVENT_ACTION2);
        }
        {
            Timer t("Try reading some bindings");
            auto lock = ecs.tecs.StartTransaction<ecs::Read<ecs::EventBindings>>();

            auto &bindings = player.Get<ecs::EventBindings>(lock);
            auto targets = bindings.Lookup(TEST_SOURCE_BUTTON);
            Assert(targets != nullptr, "Expected source button to have bindings");
            AssertEqual(targets->size(), 1u, "Unexpected binding count");
            AssertEqual(*targets->begin(),
                        std::make_pair(hand, TEST_EVENT_ACTION1),
                        "Expected button to be bound to action1");

            targets = bindings.Lookup(TEST_SOURCE_KEY);
            Assert(targets != nullptr, "Expected source key to have bindings");
            auto it = targets->begin();
            AssertEqual(*it++, std::make_pair(hand, TEST_EVENT_ACTION2), "Expected key to be bound to action2 on hand");
            AssertEqual(*it++,
                        std::make_pair(player, TEST_EVENT_ACTION2),
                        "Expected key to be bound to action2 on player");
            Assert(it == targets->end(), "Expected key to have no more bindings");
        }
        {
            Timer t("Send some test events");
            auto lock = ecs.tecs.StartTransaction<ecs::Read<ecs::EventBindings>, ecs::Write<ecs::EventInput>>();

            auto &bindings = player.Get<ecs::EventBindings>(lock);
            bindings.SendEvent(lock, TEST_SOURCE_BUTTON, player, 42);
            bindings.SendEvent(lock, TEST_SOURCE_KEY, player, 'a');
            bindings.SendEvent(lock, TEST_SOURCE_KEY, player, 'b');
        }
        {
            Timer t("Read the test events");
            auto lock = ecs.tecs.StartTransaction<ecs::Write<ecs::EventInput>>();

            ecs::Event event;
            auto &playerEvents = player.Get<ecs::EventInput>(lock);
            Assert(!playerEvents.Poll(TEST_EVENT_ACTION1, event), "Unexpected action1 event");
            AssertEqual(event.name, "", "Event data should not be set");
            AssertEqual(event.source, Tecs::Entity(), "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            Assert(playerEvents.Poll(TEST_EVENT_ACTION2, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_SOURCE_KEY, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('a'), "Unexpected event data");
            Assert(playerEvents.Poll(TEST_EVENT_ACTION2, event), "Expected to receive a second event");
            AssertEqual(event.name, TEST_SOURCE_KEY, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('b'), "Unexpected event data");
            Assert(!playerEvents.Poll(TEST_EVENT_ACTION2, event), "Unexpected third event");
            AssertEqual(event.name, "", "Event data should not be set");
            AssertEqual(event.source, Tecs::Entity(), "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            auto &handEvents = hand.Get<ecs::EventInput>(lock);
            Assert(handEvents.Poll(TEST_EVENT_ACTION1, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_SOURCE_BUTTON, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData(42), "Unexpected event data");
            Assert(!handEvents.Poll(TEST_EVENT_ACTION1, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            AssertEqual(event.source, Tecs::Entity(), "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            Assert(handEvents.Poll(TEST_EVENT_ACTION2, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_SOURCE_KEY, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('a'), "Unexpected event data");
            Assert(handEvents.Poll(TEST_EVENT_ACTION2, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_SOURCE_KEY, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('b'), "Unexpected event data");
            Assert(!handEvents.Poll(TEST_EVENT_ACTION2, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            AssertEqual(event.source, Tecs::Entity(), "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");
        }
    }

    Test test(&TrySendEvent);
} // namespace EventBindingTests
