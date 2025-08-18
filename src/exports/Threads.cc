/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Logging.hh"
#include "common/RegisteredThread.hh"

#include <strayphotons/threads.h>

SP_EXPORT void sp_thread_get_measured_fps(const char *threadName) {
    sp::GetMeasuredFps(threadName);
}
