/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpressionNode.hh"
#include "ecs/SignalManager.hh"

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
    const std::string TEST_SIGNAL_ACTION8 = "test-action8";
    const std::string TEST_SIGNAL_ACTION9 = "test-action9";

    void AssertNodeIndex(std::vector<ecs::SignalNodePtr> &nodes, ecs::SignalNodePtr node, size_t expectedIndex) {
        auto it = std::find(nodes.begin(), nodes.end(), node);
        if (it == nodes.end()) {
            Abortf("Could not find node in list: %s", node->text);
        } else {
            AssertEqual(it - nodes.begin(), expectedIndex, "Unexpected node index");
        }
    }

    void TrySetSignals() {
        ecs::SignalManager &manager = ecs::GetSignalManager();

        Tecs::Entity player, hand, unknown;
        {
            Timer t("Create a basic scene with signal values and expressions");
            auto lock = ecs::StartTransaction<ecs::AddRemove>();

            player = lock.NewEntity();
            ecs::EntityRef playerRef(ecs::Name("player", "player"), player);
            player.Set<ecs::Name>(lock, "player", "player");
            ecs::SignalRef(player, TEST_SOURCE_BUTTON).SetValue(lock, 1.0);
            ecs::SignalRef(player, TEST_SOURCE_KEY).SetValue(lock, 2.0);
            ecs::SignalRef(player, "test_a").SetValue(lock, 0.0);
            ecs::SignalRef(player, "test_b").SetValue(lock, 1.0);

            hand = lock.NewEntity();
            ecs::EntityRef handRef(ecs::Name("player", "hand"), hand);
            hand.Set<ecs::Name>(lock, "player", "hand");

            ecs::SignalRef(player, TEST_SIGNAL_ACTION1).SetBinding(lock, "player/device2_key", ecs::Name("player", ""));
            ecs::SignalRef(player, TEST_SIGNAL_ACTION2)
                .SetBinding(lock, "player/device2_key + player/device1_button", ecs::Name("player", ""));
            ecs::SignalRef(player, TEST_SIGNAL_ACTION3)
                .SetBinding(lock,
                    "-1.0 ? 0.1 : -(-1.0 + -player/device2_key) + -max(player/device1_button ? 1.2 : 0, "
                    "player:hand/test-action1)",
                    ecs::Name("player", ""));
            ecs::SignalRef(player, TEST_SIGNAL_ACTION4).SetBinding(lock, "3 +4 *2 /(1 - -5)+1 /0");
            ecs::SignalRef(player, TEST_SIGNAL_ACTION5)
                .SetBinding(lock, "cos(max(2,3)/3 *3.14159265359) * -1 ? 42 : 0.1");
            ecs::SignalRef(player, TEST_SIGNAL_ACTION6).SetBinding(lock, "(0.2 + 0.3 && 2 == 1 * 2) + 0.6 == 2 - 0.4");
            ecs::SignalRef(player, TEST_SIGNAL_ACTION7)
                .SetBinding(lock, "! 10 + 1 || !player/device2_key != !0", ecs::Name("player", ""));
            ecs::SignalRef(player, TEST_SIGNAL_ACTION8)
                .SetBinding(lock, "0 != 0.0 ? (1 ? 1 : (3 + 0.14)) : (3 + 0.14)");
            ecs::SignalRef(player, TEST_SIGNAL_ACTION9).SetBinding(lock, "");

            // Test a bunch of invalid expressions to make sure they don't crash the parser
            ecs::SignalRef testRef(player, "test");
            static const std::array invalidTestExpressions = {
                "cos(",
                "max(signal,",
                "-",
                "50 10",
                "42 max(5, 2)",
                "(hello) world",
                "1 + (",
                ")",
                "()",
                "sin()",
            };
            for (auto &exprString : invalidTestExpressions) {
                auto &expr = testRef.SetBinding(lock, exprString);
                Assertf(!expr, "Expected expression to be invalid: %s", exprString);
                AssertEqual(expr.expr, exprString, "Expected expression to be set");
            }
            std::string exprStr = "1";
            for (size_t i = 0; i < ecs::expression::MAX_SIGNAL_EXPRESSION_NODES + 1; i++) {
                exprStr += " + 1";
            }
            auto &expr = testRef.SetBinding(lock, exprStr);
            Assertf(!expr, "Expected expression node overflow to be invalid: %s", expr.rootNode->text);
            AssertEqual(expr.expr, exprStr, "Expected expression to be set");
            testRef.ClearBinding(lock);

            ecs::SignalRef(player, "test_fib")
                .SetBinding(lock, "player/test_a + player/test_b", ecs::Name("player", ""));
            ecs::SignalRef(player, "test_recurse").SetBinding(lock, "player/test_recurse + 1", ecs::Name("player", ""));

            ecs::SignalRef(hand, TEST_SIGNAL_ACTION1)
                .SetBinding(lock, "player/device1_button", ecs::Name("player", ""));
            ecs::SignalRef(hand, TEST_SIGNAL_ACTION3)
                .SetBinding(lock, "foo:unknown/device1_button", ecs::Name("player", ""));
        }
        size_t baseNodeCount = 0;
        {
            Timer t("Clear nodes from signal expression manager");
            size_t dropped = manager.DropAllUnusedNodes();
            AssertEqual(dropped, 267u, "Dropped wrong number of expression nodes");
            dropped = manager.DropAllUnusedNodes();
            AssertEqual(dropped, 0u, "Dropped wrong number of expression nodes");
            auto nodes = manager.GetNodes();
            baseNodeCount = nodes.size();
            AssertEqual(baseNodeCount, 85u, "Expected 87 remaining expression nodes");
        }
        {
            Timer t("Try looking up some bindings");
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Signals>>();

            auto &expr1 = ecs::SignalRef(player, TEST_SIGNAL_ACTION1).GetBinding(lock);
            Assert((bool)expr1, "Expected expression to be valid");
            AssertEqual(expr1.expr, "player/device2_key", "Expected expression to be set");
            auto nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr1.rootNode, 0);
            AssertEqual(expr1.rootNode->text, "player:player/device2_key", "Unexpected expression node");
            AssertTrue(std::holds_alternative<ecs::expression::SignalNode>(*nodes[0]),
                "Expected expression node to be signal");

            auto &expr2 = ecs::SignalRef(player, TEST_SIGNAL_ACTION2).GetBinding(lock);
            Assert((bool)expr2, "Expected expression to be valid");
            AssertEqual(expr2.expr, "player/device2_key + player/device1_button", "Expected expression to be set");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr2.rootNode, 3);
            AssertEqual(nodes[0]->text, "player:player/device2_key", "Unexpected expression node");
            AssertEqual(nodes[2]->text, "player:player/device1_button", "Unexpected expression node");
            AssertEqual(nodes[3]->text,
                "player:player/device2_key + player:player/device1_button",
                "Unexpected expression node");
            AssertTrue(std::holds_alternative<ecs::expression::SignalNode>(*nodes[0]),
                "Expected expression node to be signal");
            AssertTrue(std::holds_alternative<ecs::expression::SignalNode>(*nodes[2]),
                "Expected expression node to be signal");
            AssertTrue(std::holds_alternative<ecs::expression::TwoInputOperation>(*nodes[3]),
                "Expected expression node to an add operator");

            auto &expr3 = ecs::SignalRef(hand, TEST_SIGNAL_ACTION3).GetBinding(lock);
            Assert((bool)expr3, "Expected expression to be valid");
            AssertEqual(expr3.expr, "foo:unknown/device1_button", "Expected expression to be set");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr3.rootNode, 84);
            AssertEqual(nodes[84]->text, "foo:unknown/device1_button", "Unexpected expression node");
            AssertTrue(std::holds_alternative<ecs::expression::SignalNode>(*nodes[84]),
                "Expected expression node to be signal");

            auto &expr4 = ecs::SignalRef(player, TEST_SIGNAL_ACTION4).GetBinding(lock);
            Assert((bool)expr4, "Expected expression to be valid");
            AssertEqual(expr4.expr, "3 +4 *2 /(1 - -5)+1 /0");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr4.rootNode, 32);
            AssertEqual(nodes[32]->text, "3 + 4 * 2 / ( 1 - -5 ) + 1 / 0", "Unexpected expression node");

            auto &expr5 = ecs::SignalRef(player, TEST_SIGNAL_ACTION5).GetBinding(lock);
            Assert((bool)expr5, "Expected expression to be valid");
            AssertEqual(expr5.expr, "cos(max(2,3)/3 *3.14159265359) * -1 ? 42 : 0.1");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr5.rootNode, 45);
            AssertEqual(nodes[45]->text,
                "cos( max( 2 , 3 ) / 3 * 3.14159265359 ) * -1 ? 42 : 0.1",
                "Unexpected expression node");

            auto &expr6 = ecs::SignalRef(player, TEST_SIGNAL_ACTION6).GetBinding(lock);
            Assert((bool)expr6, "Expected expression to be valid");
            AssertEqual(expr6.expr, "(0.2 + 0.3 && 2 == 1 * 2) + 0.6 == 2 - 0.4");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr6.rootNode, 58);
            AssertEqual(nodes[58]->text, "( 0.2 + 0.3 && 2 == 1 * 2 ) + 0.6 == 2 - 0.4", "Unexpected expression node");

            auto &expr7 = ecs::SignalRef(player, TEST_SIGNAL_ACTION7).GetBinding(lock);
            Assert((bool)expr7, "Expected expression to be valid");
            AssertEqual(expr7.expr, "! 10 + 1 || !player/device2_key != !0");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr7.rootNode, 65);
            AssertEqual(nodes[65]->text, "0 + 1 || !player:player/device2_key != 1", "Unexpected expression node");

            auto &expr8 = ecs::SignalRef(player, TEST_SIGNAL_ACTION8).GetBinding(lock);
            Assert((bool)expr8, "Expected expression to be valid");
            AssertEqual(expr8.expr, "0 != 0.0 ? (1 ? 1 : (3 + 0.14)) : (3 + 0.14)");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr8.rootNode, 75);
            AssertEqual(nodes[66]->text, "0", "Unexpected expression node");
            AssertEqual(nodes[67]->text, "0 != 0", "Unexpected expression node");
            AssertEqual(nodes[68]->text, "1", "Unexpected expression node");
            AssertEqual(nodes[69]->text, "3", "Unexpected expression node");
            AssertEqual(nodes[70]->text, "0.14", "Unexpected expression node");
            AssertEqual(nodes[71]->text, "3 + 0.14", "Unexpected expression node");
            AssertEqual(nodes[72]->text, "( 3 + 0.14 )", "Unexpected expression node");
            AssertEqual(nodes[73]->text, "1 ? 1 : ( 3 + 0.14 )", "Unexpected expression node");
            AssertEqual(nodes[74]->text, "( 1 ? 1 : ( 3 + 0.14 ) )", "Unexpected expression node");
            AssertEqual(nodes[75]->text,
                "0 != 0 ? ( 1 ? 1 : ( 3 + 0.14 ) ) : ( 3 + 0.14 )",
                "Unexpected expression node");

            auto &expr9 = ecs::SignalRef(player, TEST_SIGNAL_ACTION9).GetBinding(lock);
            Assert((bool)expr9, "Expected expression to be valid");
            AssertEqual(expr9.expr, "");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertNodeIndex(nodes, expr9.rootNode, 76);
            AssertEqual(nodes[76]->text, "0", "Unexpected expression node");

            ecs::SignalExpression emptyExpr;
            Assert(!emptyExpr, "Expected expression to be invalid");
            AssertEqual(emptyExpr.expr, "", "Expected expression to be empty");
            nodes = manager.GetNodes();
            AssertEqual(nodes.size() - baseNodeCount, 0u, "Expected no new expression nodes");
            AssertEqual(emptyExpr.rootNode, nullptr, "Expected expression root node to be null");
        }
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

            Timer t("Try reading recursive signal binding");
            double val = ecs::SignalRef(player, "test_recurse").GetSignal(lock);
            AssertEqual(val,
                (double)ecs::MAX_SIGNAL_BINDING_DEPTH + 1.0,
                "Expected invalid signal due to depth overflow");
        }
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Write<ecs::Signals>>();

            {
                Timer t("Test calculate the fibonacci sequence");
                ecs::SignalRef fibRef(player, "test_fib");
                ecs::SignalRef aRef(player, "test_a");
                ecs::SignalRef bRef(player, "test_b");
                for (size_t i = 0; i < 1000; i++) {
                    double val = fibRef.GetSignal(lock);
                    aRef.SetValue(lock, bRef.GetSignal(lock));
                    bRef.SetValue(lock, val);
                }
                double fib = aRef.GetSignal(lock);
                Logf("1000th fibonacci number: %e", fib);
            }
        }
        {
            Timer t("Try reading some signals");
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

            double val = ecs::SignalRef(player, TEST_SIGNAL_ACTION1).GetSignal(lock);
            AssertEqual(val, 2.0, "Expected signal to match key source");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION2).GetSignal(lock);
            AssertEqual(val, 3.0, "Expected signal to match key source + button source");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION3).GetSignal(lock);
            AssertEqual(val, 1.8, "Expected signal to match complex expression");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION4).GetSignal(lock);
            // NaN and Inf values are converted to 0 to prevent poisoning all signals
            AssertEqual(val, 13.0 / 3.0, "Expected signal to match constants expression");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION5).GetSignal(lock);
            AssertEqual(val, 42.0, "Expected signal to match trig expression");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION6).GetSignal(lock);
            AssertEqual(val, 1.0, "Expected signal to match comparison expression");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION7).GetSignal(lock);
            AssertEqual(val, 1.0, "Expected signal to match comparison expression");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION8).GetSignal(lock);
            AssertEqual(val, 3.14, "Expected signal to match comparison expression");
            val = ecs::SignalRef(player, TEST_SIGNAL_ACTION9).GetSignal(lock);
            AssertEqual(val, 0.0, "Expected signal to match comparison expression");
            val = ecs::SignalRef(player, "foo").GetSignal(lock);
            AssertEqual(val, 0.0, "Expected unbound signal to have 0 value");

            val = ecs::SignalRef(hand, TEST_SIGNAL_ACTION1).GetSignal(lock);
            AssertEqual(val, 1.0, "Expected signal to match button source");
            val = ecs::SignalRef(hand, TEST_SIGNAL_ACTION3).GetSignal(lock);
            AssertEqual(val, 0.0, "Expected binding to missing entity to read as 0");
            val = ecs::SignalRef(hand, "foo").GetSignal(lock);
            AssertEqual(val, 0.0, "Expected unbound signal to have 0 value");
        }
        {
            Timer t("Add the missing unknown entity");
            auto lock = ecs::StartTransaction<ecs::AddRemove>();

            unknown = lock.NewEntity();
            ecs::EntityRef unknownRef(ecs::Name("foo", "unknown"), unknown);
            unknown.Set<ecs::Name>(lock, "foo", "unknown");

            double val = ecs::SignalRef(hand, TEST_SIGNAL_ACTION3).GetSignal(lock);
            AssertEqual(val, 0.0, "Expected binding to invalid entity to read as 0");

            val = ecs::SignalRef(hand, TEST_SIGNAL_ACTION3).GetSignal(lock);
            AssertEqual(val, 0.0, "Expected binding to missing signal to read as 0");

            ecs::SignalRef(unknownRef, TEST_SOURCE_BUTTON).SetValue(lock, 5.0);
            val = ecs::SignalRef(hand, TEST_SIGNAL_ACTION3).GetSignal(lock);
            AssertEqual(val, 5.0, "Expected binding to return signal value");
        }
    }

    Test test(&TrySetSignals);
} // namespace SignalBindingTests
