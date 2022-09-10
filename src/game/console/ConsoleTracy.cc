#include "console/CFunc.hh"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace sp {
#ifdef _WIN32
    CFunc<void> CFuncTracy("tracy", "Open tracing window", []() {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        char cmdline[128];
        strcpy(cmdline, "\"../extra/Tracy.exe\" -a 127.0.0.1");
        CreateProcessA(nullptr, cmdline, nullptr, nullptr, false, DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
    });
#endif
} // namespace sp
