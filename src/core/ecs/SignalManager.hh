/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/LockFreeMutex.hh"
#include "common/PreservingMap.hh"
#include "common/PreservingSet.hh"
#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpressionNode.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/Signals.hh"

#include <limits>
#include <memory>
#include <vector>

namespace ecs {
    class SignalManager {
        sp::LogOnExit logOnExit = "SignalManager shut down  ==============================================";

    public:
        SignalManager();

        SignalRef GetRef(const SignalKey &signal);
        SignalRef GetRef(const EntityRef &entity, const std::string_view &signalName);
        SignalRef GetRef(const std::string_view &str, const EntityScope &scope = Name());
        std::set<SignalRef> GetSignals(const std::string &search = "");
        std::set<SignalRef> GetSignals(const EntityRef &entity);

        SignalNodePtr GetNode(const expression::Node &node);
        SignalNodePtr GetConstantNode(double value);
        SignalNodePtr GetSignalNode(SignalRef ref);
        SignalNodePtr FindSignalNode(SignalRef ref);
        std::vector<SignalNodePtr> GetNodes(const std::string &search = "");

        void Tick(chrono_clock::duration maxTickInterval);
        size_t DropAllUnusedNodes();
        size_t DropAllUnusedRefs();
        size_t GetNodeCount();

    private:
        sp::LockFreeMutex mutex;
        sp::PreservingSet<expression::Node, 1000> signalNodes;
        sp::PreservingMap<SignalKey, SignalRef::Ref, 1000> signalRefs;

        sp::CFuncCollection funcs;

        friend class SignalExpression;
    };

    struct SignalRef::Ref {
        SignalKey signal;
        size_t index = std::numeric_limits<size_t>::max();

        Ref(const SignalKey &signal) : signal(signal) {}
    };

    SignalManager &GetSignalManager();
} // namespace ecs
