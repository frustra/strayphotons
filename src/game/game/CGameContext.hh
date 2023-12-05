/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "game/Game.hh"

#include <cxxopts.hpp>
#include <memory>

namespace sp {
    struct CGameContext {
        cxxopts::ParseResult options;
        Game game;
        bool disableInput;
        void *inputHandler = nullptr;

#ifdef _WIN32
        std::shared_ptr<unsigned int> winSchedulerHandle;
#endif

        CGameContext(cxxopts::ParseResult &&options, bool disableInput);
    };
} // namespace sp
