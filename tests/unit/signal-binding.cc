#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace SignalBindingTests {
    using namespace testing;

    const std::string TEST_SOURCE_BUTTON = "/device1/button";
    const std::string TEST_SOURCE_KEY = "/device2/key";
    const std::string TEST_SIGNAL_ACTION1 = "test_action1";
    const std::string TEST_SIGNAL_ACTION2 = "test_action2";
    const std::string TEST_SIGNAL_ACTION3 = "test_action3";

    void TrySetSignals() {
        Tecs::Entity player, hand, unknown;
        {
            Timer t("Create a basic scene with SignalBindings and SignalOutput components");
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            player.Set<ecs::Name>(lock, "player");
            auto &signalOutput = player.Set<ecs::SignalOutput>(lock);
            signalOutput.SetSignal(TEST_SOURCE_BUTTON, 1.0);
            signalOutput.SetSignal(TEST_SOURCE_KEY, 2.0);

            hand = lock.NewEntity();
            hand.Set<ecs::Name>(lock, "hand");
            hand.Set<ecs::SignalOutput>(lock);

            auto &playerBindings = player.Set<ecs::SignalBindings>(lock);
            playerBindings.Bind(TEST_SIGNAL_ACTION1, ecs::NamedEntity("player", player), TEST_SOURCE_KEY);
            playerBindings.Bind(TEST_SIGNAL_ACTION2, ecs::NamedEntity("player", player), TEST_SOURCE_KEY);
            playerBindings.Bind(TEST_SIGNAL_ACTION2, ecs::NamedEntity("player", player), TEST_SOURCE_BUTTON);

            auto &handBindings = hand.Set<ecs::SignalBindings>(lock);
            handBindings.Bind(TEST_SIGNAL_ACTION1, ecs::NamedEntity("player", player), TEST_SOURCE_BUTTON);
            handBindings.Bind(TEST_SIGNAL_ACTION3, ecs::NamedEntity("unknown"), TEST_SOURCE_BUTTON);
        }
        {
            Timer t("Try looking up some bindings");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::SignalBindings, ecs::SignalOutput>>();

            auto &playerBindings = player.Get<ecs::SignalBindings>(lock);
            auto sources = playerBindings.Lookup(TEST_SIGNAL_ACTION1);
            Assert(sources != nullptr, "Expected action1 signal to have bindings");
            AssertEqual(sources->size(), 1u, "Unexpected binding count");
            AssertEqual(sources->begin()->first, "player", "Expected action1 to be bound on player");
            AssertEqual(sources->begin()->second, TEST_SOURCE_KEY, "Expected action1 to be bound to key source");

            sources = playerBindings.Lookup(TEST_SIGNAL_ACTION2);
            Assert(sources != nullptr, "Expected action2 signal to have bindings");
            auto it = sources->begin();
            AssertEqual(it->first, "player", "Expected action2 to be bound on player");
            AssertEqual(it->second, TEST_SOURCE_KEY, "Expected action2 to be bound to key source");
            it++;
            AssertEqual(it->first, "player", "Expected action2 to be bound on player");
            AssertEqual(it->second, TEST_SOURCE_BUTTON, "Expected action2 to be bound to button source");
            it++;
            Assert(it == sources->end(), "Expected action2 to have no more bindings");

            auto &handBindings = hand.Get<ecs::SignalBindings>(lock);
            sources = handBindings.Lookup(TEST_SIGNAL_ACTION3);
            Assert(sources != nullptr, "Expected action3 signal to have bindings");
            AssertEqual(sources->size(), 1u, "Unexpected binding count");
            AssertEqual(sources->begin()->first, "unknown", "Expected action3 to be bound on unknown");
            AssertEqual(sources->begin()->second, TEST_SOURCE_BUTTON, "Expected action3 to be bound to button source");
        }
        {
            Timer t("Try reading some signals");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::SignalBindings, ecs::SignalOutput>>();

            auto &playerBindings = player.Get<ecs::SignalBindings>(lock);
            double val = playerBindings.GetSignal(lock, TEST_SIGNAL_ACTION1);
            AssertEqual(val, 2.0, "Expected signal to match key source");
            val = playerBindings.GetSignal(lock, TEST_SIGNAL_ACTION2);
            AssertEqual(val, 3.0, "Expected signal to match key source + button source");
            val = playerBindings.GetSignal(lock, "foo");
            AssertEqual(val, 0.0, "Expected unbound signal to have 0 value");

            auto &handBindings = hand.Get<ecs::SignalBindings>(lock);
            val = handBindings.GetSignal(lock, TEST_SIGNAL_ACTION1);
            AssertEqual(val, 1.0, "Expected signal to match button source");
            val = handBindings.GetSignal(lock, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 0.0, "Expected binding to missing entity to read as 0");
            val = handBindings.GetSignal(lock, "foo");
            AssertEqual(val, 0.0, "Expected unbound signal to have 0 value");
        }
        {
            Timer t("Add the missing unknown entity");
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            auto &handBindings = hand.Get<ecs::SignalBindings>(lock);

            unknown = lock.NewEntity();
            unknown.Set<ecs::Name>(lock, "unknown");
            double val = handBindings.GetSignal(lock, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 0.0, "Expected binding to invalid entity to read as 0");

            auto &signalOutput = unknown.Set<ecs::SignalOutput>(lock);
            val = handBindings.GetSignal(lock, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 0.0, "Expected binding to missing signal to read as 0");

            signalOutput.SetSignal(TEST_SOURCE_BUTTON, 5.0);
            val = handBindings.GetSignal(lock, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 5.0, "Expected binding to return signal value");
        }
    }

    Test test(&TrySetSignals);
} // namespace SignalBindingTests