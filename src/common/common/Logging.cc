/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Logging.hh"

namespace sp::logging {
    static Level logLevel = Level::Log;
    static chrono_clock::time_point LogEpoch = chrono_clock::now();

    float LogTime_static() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(chrono_clock::now() - LogEpoch).count() / 1000.0f;
    }

    Level GetLogLevel_static() {
        return logLevel;
    }

    void SetLogLevel_static(Level level) {
        logLevel = level;
    }

    // GlobalLogOutput_static defined in core/console/Console.cc
} // namespace sp::logging
