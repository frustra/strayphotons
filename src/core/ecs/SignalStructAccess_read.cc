/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalStructAccess_common.hh"

namespace ecs {
    double ReadStructField(const void *basePtr, const StructField &field) {
        double result = 0.0;
        detail::AccessStructField<const double>(basePtr, field, [&](const double &value) {
            result = value;
        });
        return result;
    }
} // namespace ecs
