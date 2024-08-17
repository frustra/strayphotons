/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Logging.hh"

#include <strayphotons.h>
#include <strayphotons/export.h>

using namespace sp::logging;

SP_EXPORT void sp_log_message(sp_log_level_t level, const char *message) {
    GlobalLogOutput_static(level, message);
}

SP_EXPORT void sp_set_log_level(sp_log_level_t level) {
    SetLogLevel_static(level);
}

SP_EXPORT sp_log_level_t sp_get_log_level() {
    return GetLogLevel_static();
}

SP_EXPORT void sp_set_log_output_file(const char *filePath) {
    SetLogOutputFile_static(filePath);
}

SP_EXPORT const char *sp_get_log_output_file() {
    return GetLogOutputFile_static();
}

SP_EXPORT float sp_get_log_time() {
    return LogTime_static();
}
