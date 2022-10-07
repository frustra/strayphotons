#pragma once

#include "ecs/Components.hh"

#include <bitset>

namespace ecs {
    enum class FocusLayer : uint8_t {
        Never = 0,
        Game,
        Menu,
        Overlay,
        Always,
    };

    class FocusLock {
    public:
        FocusLock(FocusLayer layer = FocusLayer::Game);

        bool AcquireFocus(FocusLayer layer);
        void ReleaseFocus(FocusLayer layer);
        bool HasPrimaryFocus(FocusLayer layer) const;
        bool HasFocus(FocusLayer layer) const;
        FocusLayer PrimaryFocus() const;

    private:
        std::bitset<static_cast<size_t>(FocusLayer::Always) - 1> layers;
    };

    std::ostream &operator<<(std::ostream &out, const FocusLock &v);

    static Component<FocusLayer> ComponentFocusLayer("focus", ComponentField::New<FocusLayer>());
} // namespace ecs

TECS_GLOBAL_COMPONENT(ecs::FocusLock);
