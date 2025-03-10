/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

namespace SignalCachingTests {
    void TryReadCachedSignal();
}
#define TEST_FRIENDS_signal_caching friend void ::SignalCachingTests::TryReadCachedSignal();

#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/SignalExpressionNode.hh"
#include "ecs/SignalManager.hh"

#include <glm/glm.hpp>
#include <tests.hh>

namespace SignalCachingTests {
    using namespace testing;

    const std::string TEST_SOURCE_BUTTON = "device1_button";
    const std::string TEST_SOURCE_KEY = "device2_key";
    const std::string TEST_SIGNAL_ACTION1 = "test-action1";
    const std::string TEST_SIGNAL_ACTION2 = "test-action2";
    const std::string TEST_SIGNAL_ACTION3 = "test-action3";

    void AssertNodeIndex(std::vector<ecs::SignalNodePtr> &nodes, ecs::SignalNodePtr node, size_t expectedIndex) {
        auto it = std::find(nodes.begin(), nodes.end(), node);
        if (it == nodes.end()) {
            Abortf("Could not find node in list: %s", node->text);
        } else {
            AssertEqual(it - nodes.begin(), expectedIndex, "Unexpected node index");
        }
    }

    void TryReadCachedSignal() {
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

            ecs::SignalRef(hand, TEST_SIGNAL_ACTION1)
                .SetBinding(lock, "player/device2_key == 42", ecs::Name("player", ""));
            ecs::SignalRef(hand, TEST_SIGNAL_ACTION2)
                .SetBinding(lock, "hand/test-action1 + player/device1_button", ecs::Name("player", ""));
            ecs::SignalRef(player, TEST_SIGNAL_ACTION3)
                .SetBinding(lock,
                    "player/device2_key > max(player/device1_button, hand/test-action1)",
                    ecs::Name("player", ""));

            AssertEqual(manager.GetNodeCount(), 8u, "Wrong number of expression nodes");
        }
        {
            Timer t("Clear nodes from signal expression manager");
            size_t dropped = manager.DropAllUnusedNodes();
            AssertEqual(dropped, 0u, "Dropped wrong number of expression nodes");
            dropped = manager.DropAllUnusedNodes();
            AssertEqual(dropped, 0u, "Dropped wrong number of expression nodes");
            AssertEqual(manager.GetNodeCount(), 8u, "Expected 8 remaining expression nodes");
        }
        {
            using namespace ecs::expression;
            Timer t("Check the signal node layout");
            auto lock = ecs::StartTransaction<ecs::Write<ecs::Signals>, ecs::ReadSignalsLock>();

            auto nodes = manager.GetNodes();
            AssertEqual(nodes.size(), 8u, "Expected no new expression nodes");
            AssertEqual(nodes[0]->text, "player:player/device2_key", "Unexpected expression node");
            AssertEqual(nodes[1]->text, "42", "Unexpected expression node");
            AssertEqual(nodes[2]->text, "player:player/device2_key == 42", "Unexpected expression node");
            AssertEqual(nodes[3]->text, "player:hand/test-action1", "Unexpected expression node");
            AssertEqual(nodes[4]->text, "player:player/device1_button", "Unexpected expression node");
            AssertEqual(nodes[5]->text,
                "player:hand/test-action1 + player:player/device1_button",
                "Unexpected expression node");
            AssertEqual(nodes[6]->text,
                "max( player:player/device1_button , player:hand/test-action1 )",
                "Unexpected expression node");
            AssertEqual(nodes[7]->text,
                "player:player/device2_key > max( player:player/device1_button , player:hand/test-action1 )",
                "Unexpected expression node");

            SignalNode *node0 = std::get_if<SignalNode>(nodes[0].get());
            ConstantNode *node1 = std::get_if<ConstantNode>(nodes[1].get());
            TwoInputOperation *node2 = std::get_if<TwoInputOperation>(nodes[2].get());
            SignalNode *node3 = std::get_if<SignalNode>(nodes[3].get());
            SignalNode *node4 = std::get_if<SignalNode>(nodes[4].get());
            TwoInputOperation *node5 = std::get_if<TwoInputOperation>(nodes[5].get());
            TwoInputOperation *node6 = std::get_if<TwoInputOperation>(nodes[6].get());
            TwoInputOperation *node7 = std::get_if<TwoInputOperation>(nodes[7].get());
            AssertTrue(node0, "Expected expression node0 to be signal");
            AssertTrue(node1, "Expected expression node1 to be constant");
            AssertTrue(node2, "Expected expression node2 to be two input op");
            AssertTrue(node3, "Expected expression node3 to be signal");
            AssertTrue(node4, "Expected expression node4 to be signal");
            AssertTrue(node5, "Expected expression node5 to be two input op");
            AssertTrue(node6, "Expected expression node6 to be two input op");
            AssertTrue(node7, "Expected expression node7 to be two input op");

            auto &expr1 = ecs::SignalRef(hand, TEST_SIGNAL_ACTION1).GetBinding(lock);
            Assert((bool)expr1, "Expected expression to be valid");
            AssertEqual(expr1.expr, "player/device2_key == 42", "Expected expression to be set");
            AssertNodeIndex(nodes, expr1.rootNode, 2);

            auto &expr2 = ecs::SignalRef(hand, TEST_SIGNAL_ACTION2).GetBinding(lock);
            Assert((bool)expr2, "Expected expression to be valid");
            AssertEqual(expr2.expr, "hand/test-action1 + player/device1_button", "Expected expression to be set");
            AssertNodeIndex(nodes, expr2.rootNode, 5);

            auto &expr3 = ecs::SignalRef(player, TEST_SIGNAL_ACTION3).GetBinding(lock);
            Assert((bool)expr3, "Expected expression to be valid");
            AssertEqual(expr3.expr,
                "player/device2_key > max(player/device1_button, hand/test-action1)",
                "Expected expression to be set");
            AssertNodeIndex(nodes, expr3.rootNode, 7);

            for (size_t i = 0; i < nodes.size(); i++) {
                auto &node = *nodes[i];
                AssertTrue(!node.uncacheable, "Expected all nodes to be cacheable");
                if (auto *signalNode = std::get_if<SignalNode>(&node)) {
                    size_t signalIndex = signalNode->signal.ptr->index;
                    std::cout << "Signal " << node.text << " = index " << signalIndex << std::endl;
                }
            }
            auto &signals = lock.Get<ecs::Signals>();
            auto checkSignals = [&](const std::map<size_t, std::optional<double>> &lastValues) {
                for (size_t index = 0; index < signals.signals.size(); index++) {
                    auto it = lastValues.find(index);
                    AssertTrue(it != lastValues.end(), "Signal index not in expected list: " + std::to_string(index));
                    auto &signal = signals.signals[index];
                    AssertTrue((bool)signal.ref, "Expected all signals to have refs");
                    if (!std::isinf(signal.value)) {
                        AssertTrue(!signal.lastValueDirty, "Expected value signal not to be dirty");
                        AssertEqual(signal.value, signal.lastValue, "Expected value signal to have correct lastValue");
                        AssertEqual(signal.value,
                            it->second,
                            "Unexpected signal value for signal index: " + std::to_string(index));
                    } else {
                        if (signal.lastValueDirty) {
                            AssertEqual(it->second,
                                std::optional<size_t>(),
                                "Unexpected signal value for signal index: " + std::to_string(index));
                        } else {
                            AssertEqual(it->second,
                                signal.lastValue,
                                "Unexpected signal value for signal index: " + std::to_string(index));
                        }
                    }
                }
            };
            auto assertSubscribers = [&](size_t index, std::initializer_list<size_t> subscriberIndexes) {
                auto &signal = signals.signals[index];
                AssertEqual(signal.subscribers.size(), subscriberIndexes.size(), "Wrong number of subscribers");
                size_t i = 0;
                for (auto &sub : signal.subscribers) {
                    auto subscriber = sub.lock();
                    if (!subscriber) continue;
                    AssertEqual(subscriber->index, *(subscriberIndexes.begin() + i), "Wrong subscriber index");
                    AssertTrue(subscriber->index < signals.signals.size(), "Wrong subscriber index");
                    bool found = false;
                    for (auto &dep : signals.signals[subscriber->index].dependencies) {
                        auto dependency = dep.lock();
                        if (!dependency) continue;
                        if (dependency->index == index) {
                            found = true;
                            break;
                        }
                    }
                    AssertTrue(found, "Subscriber does not have dependency set");
                    i++;
                }
            };
            assertSubscribers(0, {5, 6});
            assertSubscribers(1, {4, 6});
            assertSubscribers(2, {});
            assertSubscribers(3, {});
            assertSubscribers(4, {5, 6});
            assertSubscribers(5, {});
            assertSubscribers(6, {});
            checkSignals({
                {0, 1},
                {1, 2},
                {2, 0},
                {3, 1},
                {4, {}},
                {5, {}},
                {6, {}},
            });
            AssertEqual(ecs::SignalRef(hand, TEST_SIGNAL_ACTION1).GetSignal(lock),
                0.0,
                "Expected correct expression evaluation");
            checkSignals({
                {0, 1},
                {1, 2},
                {2, 0},
                {3, 1},
                {4, 0},
                {5, {}},
                {6, {}},
            });
            AssertEqual(ecs::SignalRef(hand, TEST_SIGNAL_ACTION2).GetSignal(lock),
                1.0,
                "Expected correct expression evaluation");
            checkSignals({
                {0, 1},
                {1, 2},
                {2, 0},
                {3, 1},
                {4, 0},
                {5, 1},
                {6, {}},
            });
            AssertEqual(ecs::SignalRef(player, TEST_SIGNAL_ACTION3).GetSignal(lock),
                1.0,
                "Expected correct expression evaluation");
            checkSignals({
                {0, 1},
                {1, 2},
                {2, 0},
                {3, 1},
                {4, 0},
                {5, 1},
                {6, 1},
            });
        }
    }

    Test test(&TryReadCachedSignal);
} // namespace SignalCachingTests
