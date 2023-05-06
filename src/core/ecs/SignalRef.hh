#pragma once

#include "ecs/Ecs.hh"

#include <memory>
#include <string_view>

namespace ecs {
    struct SignalKey;

    class SignalRef {
    public:
        SignalRef() {}
        SignalRef(const SignalKey &signal);
        SignalRef(const std::string_view &str, const EntityScope &scope = Name());
        SignalRef(const EntityRef &ent, const std::string_view &signalName);
        SignalRef(const SignalRef &ref) : ptr(ref.ptr) {}
        SignalRef(const std::shared_ptr<SignalKey> &ptr) : ptr(ptr) {}

        const SignalKey &Get() const;
        const EntityRef &GetEntity() const;
        const std::string &GetSignalName() const;

        explicit operator bool() const {
            return !!ptr;
        }

        bool operator==(const SignalRef &other) const {
            return ptr == other.ptr;
        }

        bool operator==(const Entity &entity) const;
        bool operator==(const std::string &signalName) const;
        bool operator<(const SignalRef &other) const;

    private:
        std::shared_ptr<SignalKey> ptr;

        friend class ReferenceManager;
        friend struct std::hash<ecs::SignalRef>;
    };
} // namespace ecs

namespace std {
    template<>
    struct hash<ecs::SignalRef> {
        std::size_t operator()(const ecs::SignalRef &ref) const;
    };
} // namespace std
