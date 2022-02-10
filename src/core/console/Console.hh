#pragma once

#include "console/CFunc.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"

#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

namespace sp {
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

        auto operator<=>(const ConsoleInputLine &other) const {
            return this->wait_until <=> other.wait_until;
        }
    };

    class ConsoleManager {
    public:
        ConsoleManager();
        void AddCVar(CVarBase *cvar);
        void RemoveCVar(CVarBase *cvar);
        void Update(bool exitOnEmptyQueue);
        void InputLoop();

        void AddLog(logging::Level lvl, const string &line);

        const vector<ConsoleLine> Lines() {
            std::lock_guard lock(linesLock);
            return outputLines;
        }

        void ParseAndExecute(const string line);
        void Execute(const string cmd, const string &args);
        void QueueParseAndExecute(const string line,
            chrono_clock::time_point wait_until = chrono_clock::now(),
            std::condition_variable *handled = nullptr);
        void AddHistory(const string &input);
        vector<string> AllHistory(size_t maxEntries);
        vector<string> AllCompletions(const string &input);

        const std::map<string, CVarBase *> CVars() {
            return cvars;
        }

    private:
        std::map<string, CVarBase *> cvars;
        std::thread cliInputThread;

        std::mutex queueLock;
        std::priority_queue<ConsoleInputLine, std::vector<ConsoleInputLine>, std::greater<ConsoleInputLine>>
            queuedCommands;

        std::mutex linesLock;
        vector<ConsoleLine> outputLines;

        std::mutex historyLock;
        vector<string> history;
    };

    ConsoleManager &GetConsoleManager();
} // namespace sp
