#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace SignalBindingTests {
    using namespace testing;

    const std::string TEST_SOURCE_BUTTON = "device1_button";
    const std::string TEST_SOURCE_KEY = "device2_key";
    const std::string TEST_SIGNAL_ACTION1 = "test_action1";
    const std::string TEST_SIGNAL_ACTION2 = "test_action2";
    const std::string TEST_SIGNAL_ACTION3 = "test_action3";

    void TrySetSignals() {
        Tecs::Entity player, hand, unknown;
        {
            Timer t("Create a basic scene with SignalBindings and SignalOutput components");
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            player.Set<ecs::Name>(lock, "", "player");
            auto &signalOutput = player.Set<ecs::SignalOutput>(lock);
            signalOutput.SetSignal(TEST_SOURCE_BUTTON, 1.0);
            signalOutput.SetSignal(TEST_SOURCE_KEY, 2.0);

            hand = lock.NewEntity();
            hand.Set<ecs::Name>(lock, "", "hand");
            hand.Set<ecs::SignalOutput>(lock);

            auto &playerBindings = player.Set<ecs::SignalBindings>(lock);
            playerBindings.Bind(TEST_SIGNAL_ACTION1, player, TEST_SOURCE_KEY);
            playerBindings.Bind(TEST_SIGNAL_ACTION2, player, TEST_SOURCE_KEY);
            playerBindings.Bind(TEST_SIGNAL_ACTION2, player, TEST_SOURCE_BUTTON);

            auto &handBindings = hand.Set<ecs::SignalBindings>(lock);
            handBindings.Bind(TEST_SIGNAL_ACTION1, player, TEST_SOURCE_BUTTON);
            handBindings.Bind(TEST_SIGNAL_ACTION3, ecs::Name("", "unknown"), TEST_SOURCE_BUTTON);
        }
        {
            Timer t("Try looking up some bindings");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::SignalBindings, ecs::SignalOutput>>();

            auto &playerBindings = player.Get<ecs::SignalBindings>(lock);
            auto bindingList = playerBindings.Lookup(TEST_SIGNAL_ACTION1);
            Assert(bindingList != nullptr, "Expected action1 signal to have bindings");
            AssertEqual(bindingList->operation,
                ecs::SignalBindings::CombineOperator::ADD,
                "Expected default combine operator");
            AssertEqual(bindingList->sources.size(), 1u, "Unexpected binding count");
            AssertEqual(bindingList->sources.begin()->first,
                ecs::Name("", "player"),
                "Expected action1 to be bound on player");
            AssertEqual(bindingList->sources.begin()->second,
                TEST_SOURCE_KEY,
                "Expected action1 to be bound to key source");

            bindingList = playerBindings.Lookup(TEST_SIGNAL_ACTION2);
            Assert(bindingList != nullptr, "Expected action1 signal to have bindings");
            AssertEqual(bindingList->operation,
                ecs::SignalBindings::CombineOperator::ADD,
                "Expected default combine operator");
            auto it = bindingList->sources.begin();
            AssertEqual(it->first, ecs::Name("", "player"), "Expected action2 to be bound on player");
            AssertEqual(it->second, TEST_SOURCE_KEY, "Expected action2 to be bound to key source");
            it++;
            AssertEqual(it->first, ecs::Name("", "player"), "Expected action2 to be bound on player");
            AssertEqual(it->second, TEST_SOURCE_BUTTON, "Expected action2 to be bound to button source");
            it++;
            Assert(it == bindingList->sources.end(), "Expected action2 to have no more bindings");

            auto &handBindings = hand.Get<ecs::SignalBindings>(lock);
            bindingList = handBindings.Lookup(TEST_SIGNAL_ACTION3);
            Assert(bindingList != nullptr, "Expected action1 signal to have bindings");
            AssertEqual(bindingList->operation,
                ecs::SignalBindings::CombineOperator::ADD,
                "Expected default combine operator");
            AssertEqual(bindingList->sources.size(), 1u, "Unexpected binding count");
            AssertEqual(bindingList->sources.begin()->first,
                ecs::Name("", "unknown"),
                "Expected action3 to be bound on unknown");
            AssertEqual(bindingList->sources.begin()->second,
                TEST_SOURCE_BUTTON,
                "Expected action3 to be bound to button source");
        }
        {
            Timer t("Try reading some signals");
            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::SignalBindings, ecs::SignalOutput, ecs::FocusLayer, ecs::FocusLock>>();

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION1);
            AssertEqual(val, 2.0, "Expected signal to match key source");
            val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION2);
            AssertEqual(val, 3.0, "Expected signal to match key source + button source");
            val = ecs::SignalBindings::GetSignal(lock, player, "foo");
            AssertEqual(val, 0.0, "Expected unbound signal to have 0 value");

            val = ecs::SignalBindings::GetSignal(lock, hand, TEST_SIGNAL_ACTION1);
            AssertEqual(val, 1.0, "Expected signal to match button source");
            val = ecs::SignalBindings::GetSignal(lock, hand, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 0.0, "Expected binding to missing entity to read as 0");
            val = ecs::SignalBindings::GetSignal(lock, hand, "foo");
            AssertEqual(val, 0.0, "Expected unbound signal to have 0 value");
        }
        {
            Timer t("Add the missing unknown entity");
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            unknown = lock.NewEntity();
            unknown.Set<ecs::Name>(lock, "", "unknown");
            double val = ecs::SignalBindings::GetSignal(lock, hand, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 0.0, "Expected binding to invalid entity to read as 0");

            auto &signalOutput = unknown.Set<ecs::SignalOutput>(lock);
            val = ecs::SignalBindings::GetSignal(lock, hand, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 0.0, "Expected binding to missing signal to read as 0");

            signalOutput.SetSignal(TEST_SOURCE_BUTTON, 5.0);
            val = ecs::SignalBindings::GetSignal(lock, hand, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 5.0, "Expected binding to return signal value");
        }
    }

    Test test(&TrySetSignals);
} // namespace SignalBindingTests
