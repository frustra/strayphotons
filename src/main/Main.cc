/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifdef _WIN32
    #include <windows.h>
#endif

#include <iostream>
using namespace std;

#include "Game.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#ifdef SP_TEST_MODE
    #include "assets/AssetManager.hh"
    #include "assets/ConsoleScript.hh"
#endif

#include <csignal>
#include <cstdio>
#include <cxxopts.hpp>
#include <filesystem>
#include <memory>
#include <strayphotons.h>

using cxxopts::value;

namespace sp {
    void handleSignals(int signal) {
        if (signal == SIGINT) {
            GameExitTriggered.test_and_set();
            GameExitTriggered.notify_all();
        }
    }
} // namespace sp

// TODO: Commented until package release saves a log file
// #if defined(_WIN32) && defined(SP_PACKAGE_RELEASE)
//     #define ARGC_NAME __argc
//     #define ARGV_NAME __argv
// int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
// #else
#define ARGC_NAME argc
#define ARGV_NAME argv
int main(int argc, char **argv)
// #endif
{

#ifdef _WIN32
    signal(SIGINT, sp::handleSignals);
#else
    struct sigaction act;
    act.sa_handler = sp::handleSignals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, 0);
#endif

    std::shared_ptr<std::remove_pointer_t<sp::StrayPhotons>> instance(sp::game_init(ARGC_NAME, ARGV_NAME),
        [](sp::StrayPhotons ctx) {
            sp::game_destroy(ctx);
        });

    return sp::game_start(instance.get());
}
