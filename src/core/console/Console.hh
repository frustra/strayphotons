/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/RegisteredThread.hh"
#include "console/CFunc.hh"
#include "console/CVar.hh"
#include "strayphotons/cpp/LockFreeMutex.hh"
#include "strayphotons/cpp/Logging.hh"

#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sp {
    class ConsoleScript;

    struct ConsoleLine {
        logging::Level level;
        std::string text;
    };

    struct ConsoleInputLine {
        std::string text;
        chrono_clock::time_point wait_until;
        std::condition_variable *handled;

        ConsoleInputLine(std::string_view text, chrono_clock::time_point wait_until, std::condition_variable *handled)
            : text(text), wait_until(wait_until), handled(handled) {}

        bool operator==(const ConsoleInputLine &other) const {
            return this->wait_until == other.wait_until;
        }

        bool operator<(const ConsoleInputLine &other) const {
            return this->wait_until < other.wait_until;
        }

        bool operator>(const ConsoleInputLine &other) const {
            return this->wait_until > other.wait_until;
        }
    };

    class ConsoleManager : public RegisteredThread {
        LogOnExit logOnExit = "ConsoleManager shut down ==============================================";

    public:
        ConsoleManager();
        void StartThread(const ConsoleScript *startupScript = nullptr);
        void Shutdown();

        void AddCVar(CVarBase *cvar);
        void RemoveCVar(CVarBase *cvar);

        CVarBase *GetCVarBase(const std::string &name) {
            return cvars[all_lower(name) ? name : to_lower_copy(name)];
        }

        template<typename T>
        CVar<T> &GetCVar(const std::string &name) {
            auto *base = GetCVarBase(name);
            Assertf(base, "CVar %s does not exist", name);
            auto *derived = dynamic_cast<CVar<T> *>(base);
            Assertf(derived, "CVar %s has unexpected type", name);
            return *derived;
        }

        void AddLine(logging::Level lvl, std::string_view line);

        template<typename Fn>
        void AccessLines(Fn &&callback) {
            std::lock_guard lock(linesLock);
            callback((const std::vector<ConsoleLine> &)outputLines);
        }

        void ParseAndExecute(std::string_view line);
        void QueueParseAndExecute(std::string_view line,
            chrono_clock::time_point wait_until = chrono_clock::now(),
            std::condition_variable *handled = nullptr);

        void AddHistory(std::string_view input);
        std::vector<std::string> AllHistory(size_t maxEntries);

        struct Completions {
            std::vector<std::string> values;
            bool pending;
        };
        Completions AllCompletions(std::string_view input, bool requestNewCompletions);

        // Starts the command line input handler thread. Must only be called once.
        void StartInputLoop();

    private:
        void Execute(std::string_view cmd, std::string_view args);

        bool ThreadInit() override;
        void Frame() override;

        void RegisterCoreCommands();
        void RegisterTracyCommands();

        LockFreeMutex cvarReadLock, cvarExecLock;
        std::map<std::string, CVarBase *> cvars;
        CFuncCollection funcs;
        std::thread cliInputThread;

        std::mutex queueLock;
        std::priority_queue<ConsoleInputLine, std::vector<ConsoleInputLine>, std::greater<ConsoleInputLine>>
            queuedCommands;

        bool exitOnEmptyQueue = false;
        std::queue<std::string> scriptCommands;

        std::mutex linesLock;
        std::vector<ConsoleLine> outputLines;

        std::mutex historyLock;
        std::vector<std::string> history;
    };

    ConsoleManager &GetConsoleManager();
} // namespace sp
