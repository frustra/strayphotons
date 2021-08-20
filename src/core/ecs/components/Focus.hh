#pragma once

#include "ecs/Components.hh"

#include <bitset>

namespace ecs {
    enum class FocusLayer : uint8_t {
        NEVER = 0,
        GAME,
        MENU,
        OVERLAY,
        ALWAYS,
    };

    class FocusLock {
    public:
        FocusLock(FocusLayer layer = FocusLayer::GAME);

        bool AcquireFocus(FocusLayer layer);
        void ReleaseFocus(FocusLayer layer);
        bool HasFocus(FocusLayer layer) const;

    private:
        std::bitset<static_cast<size_t>(FocusLayer::ALWAYS)> layers;
    };

    static Component<FocusLayer> ComponentFocusLayer("focus");

    template<>
    bool Component<FocusLayer>::Load(Lock<Read<ecs::Name>> lock, FocusLayer &dst, const picojson::value &src);
} // namespace ecs

template<>
struct Tecs::is_global_component<ecs::FocusLock> : std::true_type {};
