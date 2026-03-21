/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Focus.hh"

#include "strayphotons/cpp/Logging.hh"

namespace ecs {
    FocusLock::FocusLock(FocusLayer layer) {
        if (layer != FocusLayer::Never && layer != FocusLayer::Always) {
            AcquireFocus(layer);
        }
    }

    bool FocusLock::AcquireFocus(FocusLayer layer) {
        if (layer == FocusLayer::Never) {
            Errorf("Trying to acquire focus layer Never");
            return false;
        } else if (layer == FocusLayer::Always) {
            Errorf("Trying to acquire focus layer Always");
            return true;
        }

        size_t index = static_cast<size_t>(layer) - 1;
        layers |= 1 << index;
        return HasPrimaryFocus(layer);
    }

    void FocusLock::ReleaseFocus(FocusLayer layer) {
        if (layer == FocusLayer::Never) {
            Errorf("Trying to release focus layer Never");
            return;
        } else if (layer == FocusLayer::Always) {
            Errorf("Trying to release focus layer Always");
            return;
        }

        size_t index = static_cast<size_t>(layer) - 1;
        layers &= ~(1 << index);
    }

    bool FocusLock::HasPrimaryFocus(FocusLayer layer) const {
        if (layer == FocusLayer::Never) return false;
        if (layer == FocusLayer::Always) return true;

        size_t index = static_cast<size_t>(layer) - 1;
        for (size_t i = index + 1; i < static_cast<size_t>(FocusLayer::Always); i++) {
            if (layers & (1 << i)) return false;
        }
        return layers & (1 << index);
    }

    bool FocusLock::HasFocus(FocusLayer layer) const {
        if (layer == FocusLayer::Never) return false;
        if (layer == FocusLayer::Always) return true;

        size_t index = static_cast<size_t>(layer) - 1;
        return layers & (1 << index);
    }

    FocusLayer FocusLock::PrimaryFocus() const {
        for (size_t i = static_cast<size_t>(FocusLayer::Always) - 1; i > 0; i--) {
            if (layers & (1 << (i - 1))) return static_cast<FocusLayer>(i);
        }
        return FocusLayer::Never;
    }

    std::string FocusLock::String() const {
        std::string result;
        bool first = true;
        for (auto &layer : magic_enum::enum_values<FocusLayer>()) {
            if (layer == FocusLayer::Always) continue;
            if (HasFocus(layer)) {
                if (first) {
                    first = false;
                } else {
                    result += " ";
                }
                result += magic_enum::enum_name(layer);
            }
        }
        return result;
    }
} // namespace ecs
