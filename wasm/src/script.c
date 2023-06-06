/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <ecs/components/Transform.h>

extern void print_transform(Transform vec);

Transform add(Transform a, Transform b) {
    print_transform(a);
    print_transform(b);
    GlmVec3 pos = {5, 6, 7};
    transform_set_position(&a, &pos);
    transform_get_position(&pos, &b);
    Transform x;
    transform_from_pos(&x, &pos);
    return x;
}
