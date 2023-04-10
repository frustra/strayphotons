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
            ecs::EntityRef playerRef(ecs::Name("player", "player"), player);
            ecs::EntityRef keyboardRef(ecs::Name("input", "keyboard"), keyboard);
            ecs::EntityRef mouseRef(ecs::Name("input", "mouse"), mouse);

            player.Set<ecs::Name>(lock, "player", "player");
            auto &eventInput = player.Set<ecs::EventInput>(lock);
            eventInput.Register(lock, playerQueue, TEST_EVENT_ACTION);
            auto &signalBindings = player.Set<ecs::SignalBindings>(lock);
            signalBindings.SetBinding(TEST_SIGNAL_ACTION, "if_focused(Game, input:mouse/device1_button)");

            keyboard.Set<ecs::Name>(lock, "input", "keyboard");
            auto &eventBindings = keyboard.Set<ecs::EventBindings>(lock);
            auto &binding = eventBindings.Bind(TEST_EVENT_KEY, player, TEST_EVENT_ACTION);
            binding.actions.filterExpr = ecs::SignalExpression("is_focused(Game)");

            mouse.Set<ecs::Name>(lock, "input", "mouse");
            auto &signalOutput = mouse.Set<ecs::SignalOutput>(lock);
            signalOutput.SetSignal(TEST_SIGNAL_BUTTON, 42.0);
        }
        {
            Timer t("Try sending events and reading signals with Game focus");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock>();

            auto sentCount = ecs::EventBindings::SendEvent(lock, keyboard, ecs::Event{TEST_EVENT_KEY, keyboard, 42});
            AssertEqual(sentCount, 1, "Expected to successfully queue 1 event");

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION);
            AssertEqual(val, 42.0, "Expected signal to match button source");

            ecs::Event event;
            AssertTrue(!ecs::EventInput::Poll(lock, playerQueue, event),
                "Unexpected event, should not be visible to sender");
            AssertEqual(event.name, "", "Event data should not be set");
            AssertTrue(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::EventData(false), "Event data should not be set");
        }
        {
            Timer t("Try reading events filtered by Game focus");
            auto lock = ecs::StartTransaction<ecs::Lock<ecs::Read<ecs::EventInput>>, ecs::ReadSignalsLock>();

            ecs::Event event;
            AssertTrue(ecs::EventInput::Poll(lock, playerQueue, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_ACTION, "Unexpected event name");
            AssertEqual(event.source, keyboard, "Unexpected event source");
            AssertEqual(event.data, ecs::EventData(42), "Unexpected event data");
            AssertTrue(!ecs::EventInput::Poll(lock, playerQueue, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            AssertTrue(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::EventData(false), "Event data should not be set");
        }
        {
            Timer t("Change focus to Menu");
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();

            auto &focus = lock.Get<ecs::FocusLock>();
            AssertTrue(focus.AcquireFocus(ecs::FocusLayer::Menu), "Expected to be able to acquire menu focus");
        }
        {
            Timer t("Try sending events and reading signals with Menu focus");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock, ecs::ReadSignalsLock>();

            auto sentCount = ecs::EventBindings::SendEvent(lock, keyboard, ecs::Event{TEST_EVENT_KEY, keyboard, 42});
            AssertEqual(sentCount, 0, "Expected to not to queue any events");

            ecs::Event event;
            AssertTrue(!ecs::EventInput::Poll(lock, playerQueue, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            AssertTrue(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::EventData(false), "Event data should not be set");

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION);
            AssertEqual(val, 0.0, "Expected zero signal when out of focus");
        }
    }

    Test test(&TestSendingEventsAndSignals);
} // namespace FocusLockTests
