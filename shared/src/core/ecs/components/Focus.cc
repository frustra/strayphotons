/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Focus.hh"

#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

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
        layers.set(index);
        for (size_t i = index + 1; i < layers.size(); i++) {
            if (layers.test(i)) return false;
        }
        return true;
    }

    void FocusLock::ReleaseFocus(FocusLayer layer) {
        if (layer == FocusLayer::Never) {
            Errorf("Trying to release focus layer Never");
            return;
        } else if (layer == FocusLayer::Always) {
            Errorf("Trying to release focus layer Always");
            return;
        }

        layers.reset(static_cast<size_t>(layer) - 1);
    }

    bool FocusLock::HasPrimaryFocus(FocusLayer layer) const {
        if (layer == FocusLayer::Never) return false;
        if (layer == FocusLayer::Always) return true;

        size_t index = static_cast<size_t>(layer) - 1;
        for (size_t i = index + 1; i < layers.size(); i++) {
            if (layers.test(i)) return false;
        }
        return layers.test(index);
    }

    bool FocusLock::HasFocus(FocusLayer layer) const {
        if (layer == FocusLayer::Never) return false;
        if (layer == FocusLayer::Always) return true;

        return layers.test(static_cast<size_t>(layer) - 1);
    }

    FocusLayer FocusLock::PrimaryFocus() const {
        for (size_t i = layers.size() - 1; i >= 0; i--) {
            if (layers.test(i)) return static_cast<FocusLayer>(i + 1);
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
