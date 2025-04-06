/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <memory>
#include <string_view>

namespace ecs {
    class SignalExpression;
    struct SignalKey;

    namespace expression {
        struct Node;
    }

    using ReadSignalsLock = Lock<Read<Name, Signals, SignalOutput, SignalBindings, FocusLock>>;

    class SignalRef {
    private:
        struct Ref;

    public:
        SignalRef() {}
        SignalRef(const EntityRef &ent, const std::string_view &signalName);
        SignalRef(const std::string_view &str, const EntityScope &scope = Name());
        SignalRef(const SignalRef &ref) : ptr(ref.ptr) {}
        SignalRef(const std::shared_ptr<Ref> &ptr) : ptr(ptr) {}

        size_t &GetIndex() const;

        const EntityRef &GetEntity() const;
        const std::string &GetSignalName() const;

        std::string String() const;

        void SetScope(const EntityScope &scope);

        void AddSubscriber(const Lock<Write<Signals>> &lock, const SignalRef &ref) const;
        void MarkDirty(const Lock<Write<Signals>> &lock, size_t depth = 0) const;
        bool IsCacheable(const Lock<Read<Signals>> &lock) const;
        void RefreshUncacheable(const Lock<Write<Signals>> &lock) const;
        void UpdateDirtySubscribers(const DynamicLock<Write<Signals>, ReadSignalsLock> &lock, size_t depth = 0) const;

        double &SetValue(const Lock<Write<Signals>> &lock, double value) const;
        void ClearValue(const Lock<Write<Signals>> &lock) const;
        bool HasValue(const Lock<Read<Signals>> &lock) const;
        const double &GetValue(const Lock<Read<Signals>> &lock) const;
        SignalExpression &SetBinding(const Lock<Write<Signals>, ReadSignalsLock> &lock,
            const SignalExpression &signal) const;
        SignalExpression &SetBinding(const Lock<Write<Signals>, ReadSignalsLock> &lock,
            const std::string_view &expr,
            const EntityScope &scope = Name()) const;
        void ClearBinding(const Lock<Write<Signals>> &lock) const;
        bool HasBinding(const Lock<Read<Signals>> &lock) const;
        const SignalExpression &GetBinding(const Lock<Read<Signals>> &lock) const;

        double GetSignal(const DynamicLock<ReadSignalsLock> &lock, size_t depth = 0) const;

        explicit operator bool() const {
            return !!ptr;
        }

        bool operator==(const SignalRef &other) const {
            return ptr == other.ptr;
        }

        bool operator==(const Entity &entity) const;
        bool operator==(const EntityRef &entity) const;
        bool operator==(const std::string &signalName) const;
        bool operator<(const SignalRef &other) const;

        using WeakRef = std::weak_ptr<Ref>;
        WeakRef GetWeakRef() const {
            return ptr;
        }

    private:
        void UnsubscribeDependencies(const Lock<Write<Signals>> &lock) const;

        std::shared_ptr<Ref> ptr;

        friend class SignalManager;
        friend struct expression::Node;
        friend struct std::hash<SignalRef>;
        friend bool operator==(const std::shared_ptr<Ref> &, const WeakRef &);
#ifdef TEST_FRIENDS_signal_caching
        TEST_FRIENDS_signal_caching
#endif
    };

    using WeakSignalRef = SignalRef::WeakRef;

    // Thread-safe equality check without weak_ptr::lock()
    inline bool operator==(const std::shared_ptr<SignalRef::Ref> &a, const WeakSignalRef &b) {
        return !a.owner_before(b) && !b.owner_before(a);
    }
} // namespace ecs

namespace std {
    template<>
    struct hash<ecs::SignalRef> {
        size_t operator()(const ecs::SignalRef &ref) const;
    };
} // namespace std
