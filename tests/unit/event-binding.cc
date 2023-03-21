#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace EventBindingTests {
    using namespace testing;

    const std::string TEST_SOURCE_BUTTON = "/device1/button";
    const std::string TEST_SOURCE_KEY = "/device2/key";
    const std::string TEST_EVENT_ACTION1 = "/test/action1";
    const std::string TEST_EVENT_ACTION2 = "/test/action2";

    void TrySendEvent() {
        Tecs::Entity player, hand;
        ecs::EventQueueRef playerQueue = ecs::NewEventQueue();
        ecs::EventQueueRef handQueue1 = ecs::NewEventQueue();
        ecs::EventQueueRef handQueue2 = ecs::NewEventQueue();
        {
            Timer t("Create a basic scene with EventBindings and EventInput components");
            auto lock = ecs::StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            ecs::EntityRef playerRef(ecs::Name("", "player"), player);
            player.Set<ecs::Name>(lock, "", "player");
            auto &playerEventInput = player.Set<ecs::EventInput>(lock);
            playerEventInput.Register(lock, playerQueue, TEST_EVENT_ACTION2);

            hand = lock.NewEntity();
            ecs::EntityRef handRef(ecs::Name("", "hand"), hand);
            hand.Set<ecs::Name>(lock, "", "hand");
            auto &handEventInput = hand.Set<ecs::EventInput>(lock);
            handEventInput.Register(lock, handQueue1, TEST_EVENT_ACTION1);
            handEventInput.Register(lock, handQueue1, TEST_EVENT_ACTION2);
            handEventInput.Register(lock, handQueue2, TEST_EVENT_ACTION2);
            AssertEqual(handEventInput.events.size(), 2u, "EventInput did not save correctly");

            auto &playerBindings = player.Set<ecs::EventBindings>(lock);
            playerBindings.Bind(TEST_SOURCE_BUTTON, hand, TEST_EVENT_ACTION1);
            playerBindings.Bind(TEST_SOURCE_KEY, hand, TEST_EVENT_ACTION2);
            playerBindings.Bind(TEST_SOURCE_KEY, player, TEST_EVENT_ACTION2);
        }
        {
            Timer t("Try reading some bindings");
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventBindings>>();

            auto &bindings = player.Get<ecs::EventBindings>(lock);
            auto targets = bindings.sourceToDest.at(TEST_SOURCE_BUTTON);
            AssertEqual(targets.size(), 1u, "Unexpected binding count");
            AssertEqual(targets[0].outputs.size(), 1u, "Unexpected binding output count");
            AssertEqual(targets[0].outputs[0].target.GetLive(), hand, "Expected button to be bound on hand");
            AssertEqual(targets[0].outputs[0].queueName, TEST_EVENT_ACTION1, "Expected button to be bound to action1");

            targets = bindings.sourceToDest.at(TEST_SOURCE_KEY);
            AssertEqual(targets.size(), 1u, "Unexpected binding count");
            AssertEqual(targets[0].outputs.size(), 2u, "Unexpected binding output count");
            AssertEqual(targets[0].outputs[0].target.GetLive(), hand, "Expected key to be bound on hand");
            AssertEqual(targets[0].outputs[0].queueName, TEST_EVENT_ACTION2, "Expected key to be bound to action2");
            AssertEqual(targets[0].outputs[1].target.GetLive(), player, "Expected key to be bound on player");
            AssertEqual(targets[0].outputs[1].queueName, TEST_EVENT_ACTION2, "Expected key to be bound to action2");
        }
        {
            Timer t("Send some test events");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock>();

            auto sentCount = ecs::EventBindings::SendEvent(lock, player, ecs::Event{TEST_SOURCE_BUTTON, player, 42});
            Assert(sentCount == 1, "Expected to successfully queue 1 event");
            sentCount = ecs::EventBindings::SendEvent(lock, player, ecs::Event{TEST_SOURCE_KEY, player, 'a'});
            Assert(sentCount == 3, "Expected to successfully queue 3 events");
            sentCount = ecs::EventBindings::SendEvent(lock, player, ecs::Event{TEST_SOURCE_KEY, player, 'b'});
            Assert(sentCount == 3, "Expected to successfully queue 3 events");
        }
        {
            Timer t("Read the test events");
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput>>();

            ecs::Event event;
            Assert(!ecs::EventInput::Poll(lock, ecs::EventQueueRef(), event), "Unexpected null event queue");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            Assert(ecs::EventInput::Poll(lock, playerQueue, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION2, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('a'), "Unexpected event data");
            Assert(ecs::EventInput::Poll(lock, playerQueue, event), "Expected to receive a second event");
            AssertEqual(event.name, TEST_EVENT_ACTION2, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('b'), "Unexpected event data");
            Assert(!ecs::EventInput::Poll(lock, playerQueue, event), "Unexpected third event");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            Assert(ecs::EventInput::Poll(lock, handQueue1, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION1, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData(42), "Unexpected event data");
            Assert(ecs::EventInput::Poll(lock, handQueue1, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION2, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('a'), "Unexpected event data");
            Assert(ecs::EventInput::Poll(lock, handQueue1, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION2, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('b'), "Unexpected event data");
            Assert(!ecs::EventInput::Poll(lock, handQueue1, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            Assert(ecs::EventInput::Poll(lock, handQueue2, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION2, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('a'), "Unexpected event data");
            Assert(ecs::EventInput::Poll(lock, handQueue2, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION2, "Unexpected event name");
            AssertEqual(event.source, player, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData('b'), "Unexpected event data");
            Assert(!ecs::EventInput::Poll(lock, handQueue2, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");
        }
        {
            Timer t("Unregister event queues");
            auto lock = ecs::StartTransaction<ecs::Write<ecs::EventInput>>();

            auto &playerEventInput = player.Set<ecs::EventInput>(lock);
            playerEventInput.Unregister(playerQueue, TEST_EVENT_ACTION2);
            AssertEqual(playerEventInput.events.size(), 0u, "EventInput did not save correctly");

            auto &handEventInput = hand.Set<ecs::EventInput>(lock);
            handEventInput.Unregister(handQueue1, TEST_EVENT_ACTION1);
            handEventInput.Unregister(handQueue1, TEST_EVENT_ACTION2);
            handEventInput.Unregister(handQueue2, TEST_EVENT_ACTION2);
            AssertEqual(handEventInput.events.size(), 0u, "EventInput did not save correctly");
        }
        {
            Timer t("Send some more test events");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock>();

            auto sentCount = ecs::EventBindings::SendEvent(lock, player, ecs::Event{TEST_SOURCE_BUTTON, player, 42});
            Assert(sentCount == 0, "Expected to successfully queue 0 events");
            sentCount = ecs::EventBindings::SendEvent(lock, player, ecs::Event{TEST_SOURCE_KEY, player, 'a'});
            Assert(sentCount == 0, "Expected to successfully queue 0 events");
            sentCount = ecs::EventBindings::SendEvent(lock, player, ecs::Event{TEST_SOURCE_KEY, player, 'b'});
            Assert(sentCount == 0, "Expected to successfully queue 0 events");
        }
    }

    Test test(&TrySendEvent);
} // namespace EventBindingTests
