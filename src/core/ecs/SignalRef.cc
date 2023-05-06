#include "SignalRef.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/ReferenceManager.hh"

#include <atomic>

namespace ecs {
    SignalRef::SignalRef(const SignalKey &signal) {
        if (!signal) return;
        ptr = GetRefManager().GetSignal(signal).ptr;
    }

    SignalRef::SignalRef(const std::string_view &str, const EntityScope &scope) : SignalRef(SignalKey(str, scope)) {}
    SignalRef::SignalRef(const EntityRef &ent, const std::string_view &signalName)
        : SignalRef(SignalKey(ent, signalName)) {}

    const SignalKey &SignalRef::Get() const {
        static const SignalKey empty = {};
        return ptr ? *ptr : empty;
    }

    const EntityRef &SignalRef::GetEntity() const {
        return Get().entity;
    }

    const std::string &SignalRef::GetSignalName() const {
        return Get().signalName;
    }

    bool SignalRef::operator==(const Entity &other) const {
        if (!ptr || !other) return false;
        return ptr->entity == other;
    }

    bool SignalRef::operator==(const std::string &other) const {
        if (!ptr || other.empty()) return false;
        return ptr->signalName == other;
    }

    bool SignalRef::operator<(const SignalRef &other) const {
        return Get() < other.Get();
    }
} // namespace ecs

namespace std {
    size_t hash<ecs::SignalRef>::operator()(const ecs::SignalRef &ref) const {
        return hash<size_t>()(reinterpret_cast<size_t>(static_cast<void *>(ref.ptr.get())));
    }
} // namespace std
