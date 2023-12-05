/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "CGameContext.hh"

namespace sp {
#ifdef _WIN32
    #include <windows.h>

    std::shared_ptr<unsigned int> SetWindowsSchedulerFix() {
        // Increase thread scheduler resolution from default of 15ms
        timeBeginPeriod(1);
        return std::shared_ptr<UINT>(new UINT(1), [](UINT *period) {
            timeEndPeriod(*period);
            delete period;
        });
    }

    CGameContext::CGameContext(cxxopts::ParseResult &&options, bool disableInput)
        : options(std::move(options)), game(*this), disableInput(disableInput), inputHandler(nullptr),
          winSchedulerHandle(SetWindowsSchedulerFix()) {}
#else
    CGameContext::CGameContext(cxxopts::ParseResult &&options, bool disableInput)
        : options(std::move(options)), game(*this), disableInput(disableInput), inputHandler(nullptr) {}
#endif
} // namespace sp
