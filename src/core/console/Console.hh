#pragma once

#include "console/CFunc.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
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

        auto operator<=>(const ConsoleInputLine &other) const {
            return this->wait_until <=> other.wait_until;
        }
    };

    class ConsoleManager : public RegisteredThread {
    public:
        ConsoleManager();
        void AddCVar(CVarBase *cvar);
        void RemoveCVar(CVarBase *cvar);
        void StartThread(const ConsoleScript *startupScript);

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

        const std::map<string, CVarBase *> CVars() {
            return cvars;
        }

    private:
        void Execute(const string cmd, const string &args);

        void Frame() override;
        void InputLoop();

        std::map<string, CVarBase *> cvars;
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
    };

    ConsoleManager &GetConsoleManager();
} // namespace sp
