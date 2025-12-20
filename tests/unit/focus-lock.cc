/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Logging.hh"
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
        ecs::EventQueueRef playerQueue = ecs::EventQueue::New();
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
            ecs::SignalRef(player, TEST_SIGNAL_ACTION)
                .SetBinding(lock, "if_primary_focus(Game, input:mouse/device1_button)");

            keyboard.Set<ecs::Name>(lock, "input", "keyboard");
            auto &eventBindings = keyboard.Set<ecs::EventBindings>(lock);
            auto &binding = eventBindings.Bind(TEST_EVENT_KEY, player, TEST_EVENT_ACTION);
            binding.actions.filterExpr = ecs::SignalExpression("is_primary_focus(Game)");

            mouse.Set<ecs::Name>(lock, "input", "mouse");
            ecs::SignalRef(mouse, TEST_SIGNAL_BUTTON).SetValue(lock, 42.0);
        }
        {
            Timer t("Try sending events and reading signals with Game focus");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock>();

            auto sentCount = ecs::EventBindings::SendEvent(lock, keyboard, ecs::Event{TEST_EVENT_KEY, keyboard, 42});
            AssertEqual(sentCount, 1u, "Expected to successfully queue 1 event");

            double val = ecs::SignalRef(player, TEST_SIGNAL_ACTION).GetSignal(lock);
            AssertEqual(val, 42.0, "Expected signal to match button source");

            ecs::Event event;
            AssertTrue(!ecs::EventInput::Poll(lock, playerQueue, event),
                "Unexpected event, should not be visible to sender");
            AssertEqual(event.name.str(), "", "Event data should not be set");
            AssertTrue(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::EventData(false), "Event data should not be set");
        }
        {
            Timer t("Try reading events filtered by Game focus");
            auto lock = ecs::StartTransaction<ecs::Lock<ecs::Read<ecs::EventInput>>, ecs::ReadSignalsLock>();

            ecs::Event event;
            AssertTrue(ecs::EventInput::Poll(lock, playerQueue, event), "Expected to receive an event");
            AssertEqual(event.name.str(), TEST_EVENT_ACTION, "Unexpected event name");
            AssertEqual(event.source, keyboard, "Unexpected event source");
            AssertEqual(event.data, ecs::EventData(42), "Unexpected event data");
            AssertTrue(!ecs::EventInput::Poll(lock, playerQueue, event), "Unexpected second event");
            AssertEqual(event.name.str(), "", "Event data should not be set");
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
            AssertEqual(sentCount, 0u, "Expected to not to queue any events");

            ecs::Event event;
            AssertTrue(!ecs::EventInput::Poll(lock, playerQueue, event), "Unexpected second event");
            AssertEqual(event.name.str(), "", "Event data should not be set");
            AssertTrue(!event.source, "Event data should not be set");
            AssertEqual(event.data, ecs::EventData(false), "Event data should not be set");

            double val = ecs::SignalRef(player, TEST_SIGNAL_ACTION).GetSignal(lock);
            AssertEqual(val, 0.0, "Expected zero signal when out of focus");
        }
    }

    Test test(&TestSendingEventsAndSignals);
} // namespace FocusLockTests
