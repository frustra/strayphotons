#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"

#include <tests.hh>

namespace FocusLockTests {
    using namespace testing;

    ecs::ECS World;

    const std::string TEST_SIGNAL_BUTTON = "device1_button";
    const std::string TEST_EVENT_KEY = "/device2/key";
    const std::string TEST_SIGNAL_ACTION = "test_signal_action";
    const std::string TEST_EVENT_ACTION = "/test/event/action";

    void TestSendingEventsAndSignals() {
        Tecs::Entity player, keyboard, mouse;
        {
            Timer t("Set up player, keyboard, and mouse with event and signal bindings");
            auto lock = World.StartTransaction<ecs::AddRemove>();

            lock.Set<ecs::FocusLock>(ecs::FocusLayer::GAME);

            player = lock.NewEntity();
            keyboard = lock.NewEntity();
            mouse = lock.NewEntity();

            player.Set<ecs::Name>(lock, "", "player");
            player.Set<ecs::FocusLayer>(lock, ecs::FocusLayer::GAME);
            player.Set<ecs::EventInput>(lock, TEST_EVENT_ACTION);
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
            Timer t("Try sending events and reading signals with GAME focus");
            auto lock = World.StartTransaction<ecs::Read<ecs::Name,
                                                   ecs::SignalOutput,
                                                   ecs::SignalBindings,
                                                   ecs::EventBindings,
                                                   ecs::FocusLayer,
                                                   ecs::FocusLock>,
                ecs::Write<ecs::EventInput>>();

            auto &eventBindings = keyboard.Get<ecs::EventBindings>(lock);
            eventBindings.SendEvent(lock, TEST_EVENT_KEY, keyboard, 42);

            auto &eventInput = player.Get<ecs::EventInput>(lock);
            ecs::Event event;
            Assert(eventInput.Poll(TEST_EVENT_ACTION, event), "Expected to receive an event");
            AssertEqual(event.name, TEST_EVENT_KEY, "Unexpected event name");
            AssertEqual(event.source, ecs::Name("", "keyboard"), "Unexpected event source");
            AssertEqual(event.data, ecs::Event::EventData(42), "Unexpected event data");
            Assert(!eventInput.Poll(TEST_EVENT_ACTION, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source.Name(), "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION);
            AssertEqual(val, 42.0, "Expected signal to match button source");
        }
        {
            Timer t("Change focus to MENU");
            auto lock = World.StartTransaction<ecs::Write<ecs::FocusLock>>();

            auto &focus = lock.Get<ecs::FocusLock>();
            Assert(focus.AcquireFocus(ecs::FocusLayer::MENU), "Expected to be able to acquire menu focus");
        }
        {
            Timer t("Try sending events and reading signals with MENU focus");
            auto lock = World.StartTransaction<ecs::Read<ecs::Name,
                                                   ecs::SignalOutput,
                                                   ecs::SignalBindings,
                                                   ecs::EventBindings,
                                                   ecs::FocusLayer,
                                                   ecs::FocusLock>,
                ecs::Write<ecs::EventInput>>();

            auto &eventBindings = keyboard.Get<ecs::EventBindings>(lock);
            eventBindings.SendEvent(lock, TEST_EVENT_KEY, keyboard, 42);

            auto &eventInput = player.Get<ecs::EventInput>(lock);
            ecs::Event event;
            Assert(!eventInput.Poll(TEST_EVENT_ACTION, event), "Unexpected second event");
            AssertEqual(event.name, "", "Event data should not be set");
            Assert(!event.source.Name(), "Event data should not be set");
            AssertEqual(event.data, ecs::Event::EventData(false), "Event data should not be set");

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION);
            AssertEqual(val, 0.0, "Expected zero signal when out of focus");
        }
    }

    Test test(&TestSendingEventsAndSignals);
} // namespace FocusLockTests
