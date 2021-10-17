#include "Console.hh"

#ifndef _WIN32
    #define USE_LINENOISE_CLI
#endif

#ifdef USE_LINENOISE_CLI
    #include <linenoise.h>
#endif

#include "core/Logging.hh"
#include "core/RegisteredThread.hh"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>

namespace sp {

    ConsoleManager &GetConsoleManager() {
        static ConsoleManager GConsoleManager;
        return GConsoleManager;
    }

#ifdef USE_LINENOISE_CLI
    void LinenoiseCompletionCallback(const char *buf, linenoiseCompletions *lc);
#endif

    namespace logging {
        void GlobalLogOutput(Level lvl, const string &line) {
            GetConsoleManager().AddLog(lvl, line);
        }
    } // namespace logging

    CVarBase::CVarBase(const string &name, const string &description) : name(name), description(description) {
        GetConsoleManager().AddCVar(this);
    }

    CVarBase::~CVarBase() {
        GetConsoleManager().RemoveCVar(this);
    }

    ConsoleManager::ConsoleManager() {
        cliInputThread = std::thread([&] {
            this->InputLoop();
        });
        cliInputThread.detach();
    }

    void ConsoleManager::AddCVar(CVarBase *cvar) {
        cvars[to_lower_copy(cvar->GetName())] = cvar;
    }

    void ConsoleManager::RemoveCVar(CVarBase *cvar) {
        cvars.erase(to_lower_copy(cvar->GetName()));
    }

    void ConsoleManager::InputLoop() {
        std::mutex wait_lock;
        std::condition_variable condition;
        std::unique_lock ulock(wait_lock, std::defer_lock);

#ifdef USE_LINENOISE_CLI
        char *str;

        linenoiseHistorySetMaxLen(256);
        linenoiseSetCompletionCallback(LinenoiseCompletionCallback);

        while ((str = linenoise("sp> ")) != nullptr) {
            if (*str == '\0') {
                linenoiseFree(str);
                continue;
            }
            string line(str);

            ulock.lock();
            AddHistory(str);
            QueueParseAndExecute(line, chrono_clock::now(), &condition);
            condition.wait(ulock);
            ulock.unlock();

            linenoiseHistoryAdd(str);
            linenoiseFree(str);
        }
#else
        std::string str;

        while (std::getline(std::cin, str)) {
            if (str == "") continue;

            string line(str);

            ulock.lock();
            AddHistory(str);
            QueueParseAndExecute(line, chrono_clock::now(), &condition);
            condition.wait(ulock);
            ulock.unlock();
        }
#endif
    }

    void ConsoleManager::AddLog(logging::Level lvl, const string &line) {
        std::lock_guard lock(linesLock);
        outputLines.push_back({lvl, line});
    }

    void ConsoleManager::Update(Script *startupScript) {
        std::unique_lock<std::mutex> ulock(queueLock, std::defer_lock);

        auto now = chrono_clock::now();

        ulock.lock();
        if (startupScript != nullptr && queuedCommands.empty()) {
            ulock.unlock();
            ParseAndExecute("exit");
            return;
        }

        while (!queuedCommands.empty()) {
            auto top = queuedCommands.top();
            if (top.wait_util > now) break;

            queuedCommands.pop();
            ulock.unlock();

            ParseAndExecute(top.text);
            if (top.handled != nullptr) { top.handled->notify_all(); }

            ulock.lock();
        }
    }

    void ConsoleManager::ParseAndExecute(const string line) {
        if (line == "") return;

        auto cmd = line.begin();
        do {
            auto cmdEnd = std::find(cmd, line.end(), ';');

            std::stringstream stream(std::string(cmd, cmdEnd));
            string varName, value;
            stream >> varName;
            getline(stream, value);
            trim(value);
            Execute(varName, value);

            if (cmdEnd != line.end()) cmdEnd++;
            cmd = cmdEnd;
        } while (cmd != line.end());
    }

    void ConsoleManager::Execute(const string cmd, const string &args) {
        auto cvarit = cvars.find(to_lower_copy(cmd));
        if (cvarit != cvars.end()) {
            auto cvar = cvarit->second;
            cvar->SetFromString(args);

            if (cvar->IsValueType()) {
                logging::ConsoleWrite(logging::Level::Log, " > %s = %s", cvar->GetName(), cvar->StringValue());

                if (args.length() == 0) {
                    logging::ConsoleWrite(logging::Level::Log, " >   %s", cvar->GetDescription());
                }
            }
        } else {
            logging::ConsoleWrite(logging::Level::Log, " > '%s' undefined", cmd);
        }
    }

    void ConsoleManager::QueueParseAndExecute(const string line,
                                              chrono_clock::time_point wait_util,
                                              std::condition_variable *handled) {
        std::lock_guard lock(queueLock);
        queuedCommands.emplace(line, wait_util, handled);
    }

    void ConsoleManager::AddHistory(const string &input) {
        std::lock_guard lock(historyLock);
        if (history.size() == 0 || history[history.size() - 1] != input) { history.push_back(input); }
    }

    vector<string> ConsoleManager::AllHistory(size_t maxEntries) {
        maxEntries = std::min(maxEntries, history.size());
        if (maxEntries == 0) return {};

        std::lock_guard lock(historyLock);
        vector<string> results(history.rbegin(), history.rbegin() + maxEntries);
        return results;
    }

    vector<string> ConsoleManager::AllCompletions(const string &rawInput) {
        vector<string> results;

        auto input = to_lower_copy(rawInput);
        auto it = cvars.lower_bound(input);

        for (; it != cvars.end(); it++) {
            auto cvar = it->second;
            auto name = cvar->GetName();

            if (starts_with(to_lower_copy(name), input)) {
                results.push_back(name);
            } else {
                break;
            }
        }

        return results;
    }

#ifdef USE_LINENOISE_CLI
    void LinenoiseCompletionCallback(const char *buf, linenoiseCompletions *lc) {
        auto completions = GetConsoleManager().AllCompletions(string(buf));
        for (auto str : completions) {
            linenoiseAddCompletion(lc, str.c_str());
        }
    }
#endif
} // namespace sp

#include "ConsoleCoreCommands.hh"
