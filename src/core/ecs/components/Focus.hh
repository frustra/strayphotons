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

    std::ostream &operator<<(std::ostream &out, const FocusLayer &v);
    std::istream &operator>>(std::istream &in, FocusLayer &v);

    class FocusLock {
    public:
        FocusLock(FocusLayer layer = FocusLayer::GAME);

        bool AcquireFocus(FocusLayer layer);
        void ReleaseFocus(FocusLayer layer);
        bool HasPrimaryFocus(FocusLayer layer) const;
        bool HasFocus(FocusLayer layer) const;
        FocusLayer PrimaryFocus() const;

    private:
        std::bitset<static_cast<size_t>(FocusLayer::ALWAYS) - 1> layers;
    };

    std::ostream &operator<<(std::ostream &out, const FocusLock &v);

    static Component<FocusLayer> ComponentFocusLayer("focus");

    template<>
    bool Component<FocusLayer>::Load(sp::Scene *scene, FocusLayer &dst, const picojson::value &src);
} // namespace ecs

TECS_GLOBAL_COMPONENT(ecs::FocusLock);
