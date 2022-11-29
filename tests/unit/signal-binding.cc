#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace SignalBindingTests {
    using namespace testing;

    const std::string TEST_SOURCE_BUTTON = "device1_button";
    const std::string TEST_SOURCE_KEY = "device2_key";
    const std::string TEST_SIGNAL_ACTION1 = "test-action1";
    const std::string TEST_SIGNAL_ACTION2 = "test-action2";
    const std::string TEST_SIGNAL_ACTION3 = "test-action3";
    const std::string TEST_SIGNAL_ACTION4 = "test-action4";
    const std::string TEST_SIGNAL_ACTION5 = "test-action5";
    const std::string TEST_SIGNAL_ACTION6 = "test-action6";

    void TrySetSignals() {
        Tecs::Entity player, hand, unknown;
        {
            Timer t("Create a basic scene with SignalBindings and SignalOutput components");
            auto lock = ecs::StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            ecs::EntityRef playerRef(ecs::Name("player", "player"), player);
            player.Set<ecs::Name>(lock, "player", "player");
            auto &signalOutput = player.Set<ecs::SignalOutput>(lock);
            signalOutput.SetSignal(TEST_SOURCE_BUTTON, 1.0);
            signalOutput.SetSignal(TEST_SOURCE_KEY, 2.0);

            hand = lock.NewEntity();
            ecs::EntityRef handRef(ecs::Name("player", "hand"), hand);
            hand.Set<ecs::Name>(lock, "player", "hand");
            hand.Set<ecs::SignalOutput>(lock);

            auto &playerBindings = player.Set<ecs::SignalBindings>(lock);
            playerBindings.SetBinding(TEST_SIGNAL_ACTION1, "player/device2_key", ecs::Name("player", ""));
            playerBindings.SetBinding(TEST_SIGNAL_ACTION2,
                "player/device2_key + player/device1_button",
                ecs::Name("player", ""));
            playerBindings.SetBinding(TEST_SIGNAL_ACTION3,
                "-1.0 ? 0.1 : -(-1.0 + -player/device2_key) + -max(player/device1_button ? 1.2 : 0, "
                "player:hand/test-action1)",
                ecs::Name("player", ""));
            playerBindings.SetBinding(TEST_SIGNAL_ACTION4, "3 +4 *2 /(1 - -5)+1 /0");
            playerBindings.SetBinding(TEST_SIGNAL_ACTION5, "cos(max(2,3)/3 *3.14159265359) * -1 ? 42 : 0.1");
            playerBindings.SetBinding(TEST_SIGNAL_ACTION6, "(0.2 + 0.3 && 2 == 1 * 2) + 0.6 == 2 - 0.4");

            // Test a bunch of invalid expressions to make sure they don't crash the parser
            playerBindings.SetBinding("test", "cos(");
            playerBindings.SetBinding("test", "max(signal,");
            playerBindings.SetBinding("test", "-");
            playerBindings.SetBinding("test", "50 10");
            playerBindings.SetBinding("test", "42 max(5, 2)");
            playerBindings.SetBinding("test", "(hello) world");
            playerBindings.SetBinding("test", "1 + (");
            playerBindings.SetBinding("test", ")");
            playerBindings.SetBinding("test", "()");
            playerBindings.SetBinding("test", "sin()");
            playerBindings.SetBinding("test", "");
            playerBindings.ClearBinding("test");

            auto &handBindings = hand.Set<ecs::SignalBindings>(lock);
            handBindings.SetBinding(TEST_SIGNAL_ACTION1, "player/device1_button", ecs::Name("player", ""));
            handBindings.SetBinding(TEST_SIGNAL_ACTION3, "foo:unknown/device1_button", ecs::Name("player", ""));
        }
        /*{
            Timer t("Try looking up some bindings");
            auto lock = ecs::StartTransaction<ecs::Read<ecs::SignalBindings, ecs::SignalOutput>>();

            auto &playerBindings = player.Get<ecs::SignalBindings>(lock);
            auto &bindingExpr = playerBindings.GetBinding(TEST_SIGNAL_ACTION1);
            AssertEqual(bindingExpr.expr, "self/device2_key", "Expected expression to be set");
            AssertEqual(bindingExpr.nodes.size(), 1u, "Unexpected a single expression node");
            auto &rootNode = *bindingExpr.nodes.begin();
            Assert(std::holds_alternative<ecs::SignalExpression::SignalNode>(rootNode),
                "Expected expression node to be signal");

            bindingList = playerBindings.Lookup(TEST_SIGNAL_ACTION2);
            Assert(bindingList != nullptr, "Expected action1 signal to have bindings");
            AssertEqual(bindingList->operation,
                ecs::SignalBindings::CombineOperator::ADD,
                "Expected default combine operator");
            auto it = bindingList->sources.begin();
            AssertEqual(it->first.GetLive(), player, "Expected action2 to be bound on player");
            AssertEqual(it->second, TEST_SOURCE_KEY, "Expected action2 to be bound to key source");
            it++;
            AssertEqual(it->first.GetLive(), player, "Expected action2 to be bound on player");
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
            AssertEqual(bindingList->sources.begin()->first.Name(),
                ecs::Name("", "unknown"),
                "Expected action3 to be bound on unknown");
            Assert(!bindingList->sources.begin()->first.GetLive(), "Expected action3 to be bound on unknown");
            AssertEqual(bindingList->sources.begin()->second,
                TEST_SOURCE_BUTTON,
                "Expected action3 to be bound to button source");
        }*/
        {
            Timer t("Try reading some signals");
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

            double val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION1);
            AssertEqual(val, 2.0, "Expected signal to match key source");
            val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION2);
            AssertEqual(val, 3.0, "Expected signal to match key source + button source");
            val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION3);
            AssertEqual(val, 1.8, "Expected signal to match complex expression");
            val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION4);
            AssertEqual(val, 13.0 / 3.0, "Expected signal to match constants expression");
            val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION5);
            AssertEqual(val, 42.0, "Expected signal to match trig expression");
            val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION6);
            AssertEqual(val, 1.0, "Expected signal to match comparison expression");
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
            auto lock = ecs::StartTransaction<ecs::AddRemove>();

            unknown = lock.NewEntity();
            ecs::EntityRef unknownRef(ecs::Name("foo", "unknown"), unknown);
            unknown.Set<ecs::Name>(lock, "foo", "unknown");

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
