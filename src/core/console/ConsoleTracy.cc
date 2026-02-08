/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "console/Console.hh"

#include <unistd.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

void sp::ConsoleManager::RegisterTracyCommands() {
#ifdef _WIN32
    funcs.Register("tracy", "Open tracing window", []() {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        char cmdline[128];
        strcpy(cmdline, "\"../extra/tracy-profiler.exe\" -a 127.0.0.1");
        CreateProcessA(nullptr, cmdline, nullptr, nullptr, false, DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
    });
#else
    funcs.Register("tracy", "Open tracing window", []() {
        if (fork() == 0) { // 0 for new process, -1 error, otherwise pid
            int result = execl("../extra/tracy-profiler", "tracy-profiler", "-a", "127.0.0.1", NULL);
            // execl only returns on error
            exit(result);
        }
    });
#endif
} // namespace sp
