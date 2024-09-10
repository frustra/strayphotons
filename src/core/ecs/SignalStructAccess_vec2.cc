/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalStructAccess_common.hh"

namespace ecs {
    bool WriteStructField(void *basePtr, const StructField &field, std::function<void(glm::dvec2 &)> accessor) {
        return detail::AccessStructField<glm::dvec2>(basePtr, field, accessor);
    }
} // namespace ecs
