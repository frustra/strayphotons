/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Tracing.hh"

#include <cstdlib>

// void *operator new(size_t size) {
//     auto *ptr = std::malloc(size);
//     // This holds a shared_mutex internally that causes multiple threads to block on malloc
//     TracyAlloc(ptr, size);
//     return ptr;
// }

// void operator delete(void *ptr) {
//     TracyFree(ptr);
//     std::free(ptr);
// }
