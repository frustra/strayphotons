/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalStructAccess_common.hh"
#include "ecs/EntityRef.hh"
#include "ecs/StructMetadata.hh"

namespace ecs {
    double ReadStructField(const void *basePtr, const StructField &field) {
        double result = 0.0;
        // Hack to prevent EventData filters from failing to evaluate
        if (field.type == typeid(Entity)) {
            const Entity &value = field.Access<Entity>(basePtr);
            return (bool)value ? 1.0 : 0.0;
        } else if (field.type == typeid(NamedEntity)) {
            const NamedEntity &value = field.Access<NamedEntity>(basePtr);
            return value.IsValid() ? 1.0 : 0.0;
        } else {
            detail::AccessStructField<const double>(basePtr, field, [&](const double &value) {
                result = value;
            });
        }
        return result;
    }
} // namespace ecs
