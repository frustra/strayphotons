/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"

#ifdef __cplusplus

namespace sp {
    class RegisteredThread;
} // namespace sp

extern "C" {
typedef sp::RegisteredThread sp_thread_t;
#else
typedef void sp_thread_t;
#endif

// The following functions are declared in src/exports/Threads.cc

SP_EXPORT void sp_thread_get_measured_fps(const char *threadName);

#ifdef __cplusplus
}
#endif
