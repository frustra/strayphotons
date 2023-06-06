/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CFunc.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"

#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

namespace sp {
    class ConsoleScript;

    struct ConsoleLine {
        logging::Level level;
        string text;
    };

    struct ConsoleInputLine {
        string text;
        chrono_clock::time_point wait_until;
        std::condition_variable *handled;

        ConsoleInputLine(string text, chrono_clock::time_point wait_until, std::condition_variable *handled)
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
        void StartThread(const ConsoleScript *startupScript);
        void Shutdown();

        void AddCVar(CVarBase *cvar);
        void RemoveCVar(CVarBase *cvar);

        template<typename T>
        CVar<T> &GetCVar(const string &name) {
            auto *base = cvars[all_lower(name) ? name : to_lower_copy(name)];
            Assertf(base, "CVar %s does not exist", name);
            auto *derived = dynamic_cast<CVar<T> *>(base);
            Assertf(derived, "CVar %s has unexpected type", name);
            return *derived;
        }

        void AddLog(logging::Level lvl, const string &line);

        const vector<ConsoleLine> Lines() {
            std::lock_guard lock(linesLock);
            return outputLines;
        }

        void ParseAndExecute(const string line);
        void QueueParseAndExecute(const string line,
            chrono_clock::time_point wait_until = chrono_clock::now(),
            std::condition_variable *handled = nullptr);

        void AddHistory(const string &input);
        vector<string> AllHistory(size_t maxEntries);

        struct Completions {
            vector<string> values;
            bool pending;
        };
        Completions AllCompletions(const string &input, bool requestNewCompletions);

        // Starts the command line input handler thread. Must only be called once.
        void StartInputLoop();

    private:
        void Execute(const string cmd, const string &args);

        void Frame() override;

        void RegisterCoreCommands();
        void RegisterTracyCommands();

        LockFreeMutex cvarReadLock, cvarExecLock;
        std::map<string, CVarBase *> cvars;
        CFuncCollection funcs;
        std::thread cliInputThread;

        std::mutex queueLock;
        std::priority_queue<ConsoleInputLine, std::vector<ConsoleInputLine>, std::greater<ConsoleInputLine>>
            queuedCommands;

        bool exitOnEmptyQueue = false;
        std::queue<std::string> scriptCommands;

        std::mutex linesLock;
        vector<ConsoleLine> outputLines;

        std::mutex historyLock;
        vector<string> history;

        friend ConsoleManager &GetConsoleManager();
    };

    ConsoleManager &GetConsoleManager();
} // namespace sp
