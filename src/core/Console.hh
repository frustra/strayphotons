#pragma once

#include "CFunc.hh"
#include "CVar.hh"
#include "Logging.hh"

#include <Common.hh>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

namespace sp {
    class Script;

    struct ConsoleLine {
        logging::Level level;
        string text;
    };

    struct ConsoleInputLine {
        string text;
        chrono_clock::time_point wait_util;
        std::condition_variable *handled;

        ConsoleInputLine(string text, chrono_clock::time_point wait_util, std::condition_variable *handled)
            : text(text), wait_util(wait_util), handled(handled) {}

        bool operator<(const ConsoleInputLine &other) const {
            return this->wait_util < other.wait_util;
        }
    };

    class ConsoleManager {
    public:
        ConsoleManager();
        void AddCVar(CVarBase *cvar);
        void RemoveCVar(CVarBase *cvar);
        void Update(Script *startupScript = nullptr);
        void InputLoop();

        void AddLog(logging::Level lvl, const string &line);

        inline vector<ConsoleLine> Lines() {
            const std::lock_guard<std::mutex> lock(outputLock);

            return std::vector<ConsoleLine>(outputLines.begin(), outputLines.end());
        }

        void ParseAndExecute(const string line);
        void Execute(const string cmd, const string &args);
        void QueueParseAndExecute(const string line,
                                  chrono_clock::time_point wait_util = chrono_clock::now(),
                                  std::condition_variable *handled = nullptr);
        void AddHistory(const string &input);
        string GetHistory(size_t index);
        string AutoComplete(const string &input);
        vector<string> AllCompletions(const string &input);

        const std::map<string, CVarBase *> CVars() {
            return cvars;
        }

    private:
        std::map<string, CVarBase *> cvars;
        std::thread cliInputThread;

        std::mutex queueLock;
        std::priority_queue<ConsoleInputLine> queuedCommands;
        std::mutex outputLock;
        vector<ConsoleLine> outputLines;

        std::mutex historyLock;
        vector<string> history;
    };

    ConsoleManager &GetConsoleManager();
} // namespace sp
