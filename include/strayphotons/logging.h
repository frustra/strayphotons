/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"

#ifdef __cplusplus
namespace sp::logging {
    enum class Level : uint8_t;
}

extern "C" {
typedef sp::logging::Level sp_log_level_t;
#else
const uint8_t SP_LOG_LEVEL_ERROR = 0;
const uint8_t SP_LOG_LEVEL_WARN = 1;
const uint8_t SP_LOG_LEVEL_LOG = 2;
const uint8_t SP_LOG_LEVEL_DEBUG = 3;
const uint8_t SP_LOG_LEVEL_TRACE = 4;
typedef uint8_t sp_log_level_t;
#endif

// The following functions are declared in src/exports/Logging.cc

// Add a message to the global console log.
SP_EXPORT void sp_log_message(sp_log_level_t level, const char *message);

// Sets the threshold level at which log messages are printed.
SP_EXPORT void sp_set_log_level(sp_log_level_t level);
SP_EXPORT sp_log_level_t sp_get_log_level();

SP_EXPORT void sp_set_log_output_file(const char *filePath);
SP_EXPORT const char *sp_get_log_output_file();

// Returns the number of seconds since application start.
SP_EXPORT float sp_get_log_time();

#ifdef __cplusplus
}
#endif
