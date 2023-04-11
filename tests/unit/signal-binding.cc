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
    const std::string TEST_SIGNAL_ACTION7 = "test-action7";

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
            signalOutput.SetSignal("test_a", 0.0);
            signalOutput.SetSignal("test_b", 1.0);

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
            playerBindings.SetBinding(TEST_SIGNAL_ACTION7,
                "! 10 + 1 || !player/device2_key != !0",
                ecs::Name("player", ""));

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

            playerBindings.SetBinding("test_fib", "player/test_a + player/test_b", ecs::Name("player", ""));
            playerBindings.SetBinding("test_recurse", "player/test_recurse + 1", ecs::Name("player", ""));

            auto &handBindings = hand.Set<ecs::SignalBindings>(lock);
            handBindings.SetBinding(TEST_SIGNAL_ACTION1, "player/device1_button", ecs::Name("player", ""));
            handBindings.SetBinding(TEST_SIGNAL_ACTION3, "foo:unknown/device1_button", ecs::Name("player", ""));
        }
        {
            Timer t("Try looking up some bindings");
            auto lock = ecs::StartTransaction<ecs::Read<ecs::SignalBindings, ecs::SignalOutput>>();

            auto &playerBindings = player.Get<ecs::SignalBindings>(lock);
            auto &expr1 = playerBindings.GetBinding(TEST_SIGNAL_ACTION1);
            AssertEqual(expr1.expr, "player/device2_key", "Expected expression to be set");
            AssertEqual(expr1.nodes.size(), 1u, "Expected a single expression node");
            AssertEqual(expr1.rootIndex, 0, "Expected expression root node to be 0");
            AssertEqual(expr1.nodeStrings[0], "player:player/device2_key", "Unexpected expression node");
            AssertTrue(std::holds_alternative<ecs::SignalExpression::SignalNode>(expr1.nodes[0]),
                "Expected expression node to be signal");

            auto &expr2 = playerBindings.GetBinding(TEST_SIGNAL_ACTION2);
            AssertEqual(expr2.expr, "player/device2_key + player/device1_button", "Expected expression to be set");
            AssertEqual(expr2.nodes.size(), 3u, "Expected 3 expression nodes");
            AssertEqual(expr2.rootIndex, 2, "Expected expression root node to be 2");
            AssertEqual(expr2.nodeStrings[0], "player:player/device2_key", "Unexpected expression node");
            AssertEqual(expr2.nodeStrings[1], "player:player/device1_button", "Unexpected expression node");
            AssertEqual(expr2.nodeStrings[2],
                "player:player/device2_key + player:player/device1_button",
                "Unexpected expression node");
            AssertTrue(std::holds_alternative<ecs::SignalExpression::SignalNode>(expr2.nodes[0]),
                "Expected expression node to be signal");
            AssertTrue(std::holds_alternative<ecs::SignalExpression::SignalNode>(expr2.nodes[1]),
                "Expected expression node to be signal");
            AssertTrue(std::holds_alternative<ecs::SignalExpression::TwoInputOperation>(expr2.nodes[2]),
                "Expected expression node to an add operator");

            auto &expr4 = playerBindings.GetBinding(TEST_SIGNAL_ACTION4);
            AssertEqual(expr4.expr, "3 +4 *2 /(1 - -5)+1 /0");
            AssertEqual(expr4.nodes.size(), 13u, "Expected 13 expression nodes");
            AssertEqual(expr4.rootIndex, 12, "Expected expression root node to be 12");
            AssertEqual(expr4.nodeStrings[expr4.rootIndex],
                "3 + 4 * 2 / ( 1 - -5 ) + 1 / 0",
                "Unexpected expression node");

            auto &expr5 = playerBindings.GetBinding(TEST_SIGNAL_ACTION5);
            AssertEqual(expr5.expr, "cos(max(2,3)/3 *3.14159265359) * -1 ? 42 : 0.1");
            AssertEqual(expr5.nodes.size(), 13u, "Expected 13 expression nodes");
            AssertEqual(expr5.rootIndex, 12, "Expected expression root node to be 12");
            AssertEqual(expr5.nodeStrings[expr5.rootIndex],
                "cos( max( 2 , 3 ) / 3 * 3.14159265359 ) * -1 ? 42 : 0.1",
                "Unexpected expression node");

            auto &expr6 = playerBindings.GetBinding(TEST_SIGNAL_ACTION6);
            AssertEqual(expr6.expr, "(0.2 + 0.3 && 2 == 1 * 2) + 0.6 == 2 - 0.4");
            AssertEqual(expr6.nodes.size(), 13u, "Expected 15 expression nodes with 2 optimized out");
            AssertEqual(expr6.rootIndex, 12, "Expected expression root node to be 14");
            AssertEqual(expr6.nodeStrings[expr6.rootIndex],
                "( 0.2 + 0.3 && 2 == 1 * 2 ) + 0.6 == 2 - 0.4",
                "Unexpected expression node");

            auto &expr7 = playerBindings.GetBinding(TEST_SIGNAL_ACTION7);
            AssertEqual(expr7.expr, "! 10 + 1 || !player/device2_key != !0");
            AssertEqual(expr7.nodes.size(), 8u, "Expected 8 expression nodes");
            AssertEqual(expr7.rootIndex, 7, "Expected expression root node to be 7");
            AssertEqual(expr7.nodeStrings[expr7.rootIndex],
                "!10 + 1 || !player:player/device2_key != 1",
                "Unexpected expression node");

            auto &handBindings = hand.Get<ecs::SignalBindings>(lock);
            auto &expr3 = handBindings.GetBinding(TEST_SIGNAL_ACTION3);
            AssertEqual(expr3.expr, "foo:unknown/device1_button", "Expected expression to be set");
            AssertEqual(expr3.nodes.size(), 1u, "Expected a single expression node");
            AssertEqual(expr3.rootIndex, 0, "Expected expression root node to be 0");
            AssertEqual(expr3.nodeStrings[0], "foo:unknown/device1_button", "Unexpected expression node");
            AssertTrue(std::holds_alternative<ecs::SignalExpression::SignalNode>(expr3.nodes[0]),
                "Expected expression node to be signal");
        }
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

            Timer t("Try reading recursive signal binding");
            double val = ecs::SignalBindings::GetSignal(lock, player, "test_recurse");
            AssertEqual(val,
                (double)ecs::MAX_SIGNAL_BINDING_DEPTH + 1.0,
                "Expected invalid signal due to depth overflow");
        }
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Write<ecs::SignalOutput>>();

            auto &signals = player.Get<ecs::SignalOutput>(lock);

            {
                Timer t("Test calculate the fibonacci sequence");
                for (size_t i = 0; i < 1000; i++) {
                    double val = ecs::SignalBindings::GetSignal(lock, player, "test_fib");
                    signals.SetSignal("test_a", signals.GetSignal("test_b"));
                    signals.SetSignal("test_b", val);
                }
                double fib = signals.GetSignal("test_a");
                Logf("1000th fibonacci number: %e", fib);
            }
        }
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
            val = ecs::SignalBindings::GetSignal(lock, player, TEST_SIGNAL_ACTION7);
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
