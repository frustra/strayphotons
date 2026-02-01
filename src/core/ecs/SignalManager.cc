/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalManager.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/SignalRef.hh"

#include <mutex>
#include <picojson.h>
#include <shared_mutex>

namespace ecs {
    using namespace expression;

    SignalManager &GetSignalManager() {
        static SignalManager signalManager;
        return signalManager;
    }

    SignalManager::SignalManager() {
        funcs.Register<std::string>("assert_signal",
            "Asserts a signal expression evaluates to true (i.e. >= 0.5) (assert_signal <expr>)",
            [](std::string input) {
                auto lock = StartTransaction<ReadAll>();
                SignalExpression expr(input);
                double result = expr.Evaluate(lock);
                if (result < 0.5) {
                    Abortf("Assertion failed (%s): %f != true", expr.expr, result);
                }
            });
        funcs.Register("debug_signals", "Prints debug information about the signal manager", [this] {
            auto lock = StartTransaction<Read<Signals>>();
            auto &signals = lock.Get<Signals>();
            Logf("Signal debug info:");
            Logf("  Storage capacity: %llu", signals.signals.capacity());
            Logf("  Allocated signals: %llu/%llu",
                signals.signals.size() - signals.freeIndexes.size(),
                signals.signals.size());
            Logf("  Free signals: %llu", signals.freeIndexes.size());
            Logf("  Dirty signals: %llu", signals.dirtyIndices.size());
            size_t minIndex = ~0llu;
            size_t maxIndex = 0;
            for (size_t i = 0; i < signals.signals.size(); i++) {
                auto &signal = signals.signals[i];
                if (signal.ref) {
                    minIndex = std::min(minIndex, i);
                    maxIndex = std::max(maxIndex, i);
                }
            }
            Logf("  First/Last used index: %llu/%llu", minIndex, maxIndex);
            Logf("  Signal References: %llu", signalRefs.Size());
        });
    }

    SignalRef SignalManager::GetRef(const SignalKey &signal) {
        if (!signal) return SignalRef();

        SignalRef ref = signalRefs.Load(signal);
        if (!ref) {
            std::lock_guard lock(mutex);
            ref = signalRefs.Load(signal);
            if (ref) return ref;

            ref = make_shared<SignalRef::Ref>(signal);
            signalRefs.Register(signal, ref.ptr);
        }
        return ref;
    }

    SignalRef SignalManager::GetRef(const EntityRef &entity, const std::string_view &signalName) {
        return GetRef(SignalKey{entity, signalName});
    }

    SignalRef SignalManager::GetRef(const std::string_view &str, const EntityScope &scope) {
        return GetRef(SignalKey{str, scope});
    }

    std::set<SignalRef> SignalManager::GetSignals(const std::string &search) {
        std::set<SignalRef> results;
        signalRefs.ForEach([&](const SignalKey &signal, std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (search.empty() || signal.String().find(search) != std::string::npos) {
                results.emplace(refPtr);
            }
        });
        return results;
    }

    std::set<SignalRef> SignalManager::GetSignals(const EntityRef &entity) {
        std::set<SignalRef> results;
        signalRefs.ForEach([&](const SignalKey &signal, std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (signal.entity == entity) {
                results.emplace(refPtr);
            }
        });
        return results;
    }

    SignalNodePtr SignalManager::GetNode(const Node &node) {
        return Node::AddReferences(signalNodes.LoadOrInsert(node));
    }

    SignalNodePtr SignalManager::GetConstantNode(double value) {
        return GetNode(Node{ConstantNode{value}, picojson::value(value).serialize()});
    }

    SignalNodePtr SignalManager::GetSignalNode(SignalRef ref) {
        if (!ref) return GetConstantNode(0.0);
        return GetNode(Node{SignalNode{ref}, ref.String()});
    }

    SignalNodePtr SignalManager::FindSignalNode(SignalRef ref) {
        return signalNodes.Find(Node{SignalNode{ref}, ""});
    }

    std::vector<SignalNodePtr> SignalManager::GetNodes(const std::string &search) {
        std::vector<SignalNodePtr> results;
        signalNodes.ForEach([&](const Node &node, SignalNodePtr ptr) {
            if (search.empty() || node.text.find(search) != std::string::npos) {
                results.emplace_back(ptr);
            }
        });
        return results;
    }

    void SignalManager::Tick(chrono_clock::duration maxTickInterval) {
        signalNodes.Tick(maxTickInterval);

        std::vector<std::shared_ptr<SignalRef::Ref>> refsToFree;
        signalRefs.Tick(maxTickInterval, [&](std::shared_ptr<SignalRef::Ref> &refPtr) {
            if (!refPtr) return;
            refsToFree.emplace_back(refPtr);
        });
        if (!refsToFree.empty()) {
            ZoneScopedN("FreeSignals");
            auto lock = StartTransaction<Write<Signals>>();
            auto &signals = lock.Get<Signals>();
            for (auto &refPtr : refsToFree) {
                signals.FreeSignal(lock, refPtr->index);
                refPtr->index = std::numeric_limits<size_t>::max();
            }
        }
    }

    size_t SignalManager::DropAllUnusedNodes() {
        return signalNodes.DropAll();
    }

    size_t SignalManager::DropAllUnusedRefs() {
        std::vector<std::shared_ptr<SignalRef::Ref>> refsToFree;
        signalRefs.DropAll([&](std::shared_ptr<SignalRef::Ref> &refPtr) {
            refsToFree.emplace_back(refPtr);
        });
        if (!refsToFree.empty()) {
            ZoneScopedN("FreeSignals");
            QueueTransaction<Write<Signals>>([refsToFree](const Lock<Write<Signals>> &lock) {
                auto &signals = lock.Get<Signals>();
                for (auto &refPtr : refsToFree) {
                    signals.FreeSignal(lock, refPtr->index);
                    refPtr->index = std::numeric_limits<size_t>::max();
                }
            });
        }
        return refsToFree.size();
    }

    size_t SignalManager::GetNodeCount() {
        return signalNodes.Size();
    }
} // namespace ecs
