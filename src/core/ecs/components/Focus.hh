/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"

#include <bitset>

namespace ecs {
    enum class FocusLayer {
        Never = 0,
        Game,
        Menu,
        Overlay,
        Always,
    };

    class FocusLock {
    public:
        FocusLock() : FocusLock(FocusLayer::Game) {}
        explicit FocusLock(FocusLayer layer);

        bool AcquireFocus(FocusLayer layer);
        void ReleaseFocus(FocusLayer layer);
        bool HasPrimaryFocus(FocusLayer layer) const;
        bool HasFocus(FocusLayer layer) const;
        FocusLayer PrimaryFocus() const;

        std::string String() const;

    private:
        std::bitset<static_cast<size_t>(FocusLayer::Always) - 1> layers;
    };

    std::ostream &operator<<(std::ostream &out, const FocusLock &v);
} // namespace ecs

TECS_GLOBAL_COMPONENT(ecs::FocusLock);
