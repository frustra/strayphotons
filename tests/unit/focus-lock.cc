#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"

#include <tests.hh>

namespace FocusLockTests {
    using namespace testing;

    const std::string TEST_SIGNAL_BUTTON = "device1_button";
    const std::string TEST_EVENT_KEY = "/device2/key";
    const std::string TEST_SIGNAL_ACTION = "test_signal_action";
    const std::string TEST_EVENT_ACTION = "/test/event/action";

    void TestSendingEventsAndSignals() {
        Tecs::Entity player, keyboard, mouse;
        ecs::EventQueueRef playerQueue = ecs::NewEventQueue();
        {
            Timer t("Set up player, keyboard, and mouse with event and signal bindings");
            auto lock = ecs::StartTransaction<ecs::AddRemove>();

            lock.Set<ecs::FocusLock>(ecs::FocusLayer::Game);

            player = lock.NewEntity();
            keyboard = lock.NewEntity();
            mouse = lock.NewEntity();
            ecs::EntityRef playerRef(ecs::Name("", "player"), player);
            ecs::EntityRef keyboardRef(ecs::Name("", "keyboard"), keyboard);
            ecs::EntityRef mouseRef(ecs::Name("", "mouse"), mouse);

            player.Set<ecs::Name>(lock, "", "player");
            player.Set<ecs::FocusLayer>(lock, ecs::FocusLayer::Game);
            auto &eventInput = player.Set<ecs::EventInput>(lock);
            eventInput.Register(playerQueue, TEST_EVENT_ACTION);
            auto &signalBindings = player.Set<ecs::SignalBindings>(lock);
            signalBindings.Bind(TEST_SIGNAL_ACTION, mouse, TEST_SIGNAL_BUTTON);

            keyboard.Set<ecs::Name>(lock, "", "keyboard");
            auto &eventBindings = keyboard.Set<ecs::EventBindings>(lock);
            eventBindings.Bind(TEST_EVENT_KEY, player, TEST_EVENT_ACTION);

            mouse.Set<ecs::Name>(lock, "", "mouse");
            auto &signalOutput = mouse.Set<ecs::SignalOutput>(lock);
            signalOutput.SetSignal(TEST_SIGNAL_BUTTON, 42.0);
        }
        {
            Timer t("Try sending events and reading signals with Game focus");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock, ecs::ReadSignalsLock>();

            auto sentCount = ecs::EventBindings::SendEvent(lock, TEST_EVENT_KEY, keyboard, 42);
            Assert(sentCount == 1, "Expected to successfully queue 1 event");

            ecs::Event event;
            Assert(ecs::EventInput::Poll(lock, playerQueue, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION, "Unexpected event name");
            AssertEqual(event.source, keyboard, "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData(42), "Unexpected event data");
            Assert(!ecs::EventInput::Poll(lock, playerQueue, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION);
            AssertEqual(val, 42.0, "Expected signal to match button source");
        }
        {
            Timer t("Change focus to Menu");
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();

            auto &focus = lock.Get<ecs::FocusLock>();
            Assert(focus.AcquireFocus(ecs::FocusLayer::Menu), "Expected to be able to acquire menu focus");
        }
        {
            Timer t("Try sending events and reading signals with Menu focus");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock, ecs::ReadSignalsLock>();

            auto sentCount = ecs::EventBindings::SendEvent(lock, TEST_EVENT_KEY, keyboard, 42);
            Assert(sentCount == 0, "Expected to not to queue any events");

            ecs::Event event;
            Assert(!ecs::EventInput::Poll(lock, playerQueue, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION);
            AssertEqual(val, 0.0, "Expected zero signal when out of focus");
        }
    }

    Test test(&TestSendingEventsAndSignals);
} // namespace FocusLockTests
