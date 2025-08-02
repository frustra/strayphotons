/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Tracing.hh"

// #include <atomic>
// #include <cstdlib>

// thread_local std::atomic_size_t allocationCounter;

// size_t SampleAllocationCount() {
//     return allocationCounter.exchange(0u);
// }

// void *operator new(size_t size) {
//     allocationCounter++;
//     auto *ptr = std::malloc(size);
//     // This holds a shared_mutex internally that causes multiple threads to block on malloc
//     TracyAlloc(ptr, size);
//     return ptr;
// }

// void operator delete(void *ptr) {
//     TracyFree(ptr);
//     std::free(ptr);
// }
