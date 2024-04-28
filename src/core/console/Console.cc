/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Console.hh"

#ifndef _WIN32
    #define USE_LINENOISE_CLI
#endif

#ifdef USE_LINENOISE_CLI
    #include <linenoise.h>
#endif

#include "assets/ConsoleScript.hh"
#include "common/Logging.hh"
#include "common/RegisteredThread.hh"
#include "common/Tracing.hh"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <sstream>

namespace sp {
    ConsoleManager &GetConsoleManager() {
        static ConsoleManager consoleManager;
        return consoleManager;
    }

#ifdef USE_LINENOISE_CLI
    void LinenoiseCompletionCallback(const char *buf, linenoiseCompletions *lc);
#endif

    namespace logging {
        void GlobalLogOutput_static(Level level, const string &message) {
            if (level > GetLogLevel_static()) return;
            std::cout << message;
            if (level < Level::Debug) {
                GetConsoleManager().AddLine(level, message);
            }
        }
    } // namespace logging

    CVarBase::CVarBase(const string &name, const string &description)
        : name(name), nameLower(to_lower_copy(name)), description(description) {}

    void CVarBase::Register() {
        GetConsoleManager().AddCVar(this);
    }

    void CVarBase::UnRegister() {
        GetConsoleManager().RemoveCVar(this);
    }

    ConsoleManager::ConsoleManager() : RegisteredThread("ConsoleManager", 60.0) {}

    void ConsoleManager::Shutdown() {
        StopThread();
    }

    void ConsoleManager::AddCVar(CVarBase *cvar) {
        std::lock_guard lock(cvarReadLock);
        cvars[cvar->GetNameLower()] = cvar;
    }

    void ConsoleManager::RemoveCVar(CVarBase *cvar) {
        std::lock_guard execLock(cvarExecLock);
        std::lock_guard readLock(cvarReadLock);
        cvars.erase(cvar->GetNameLower());
    }

    void ConsoleManager::StartInputLoop() {
        Assertf(cliInputThread.get_id() == std::thread::id(), "Console input thread already started");

        cliInputThread = std::thread([this] {
            tracy::SetThreadName("ConsoleManager::InputLoop");

            std::mutex wait_lock;
            std::condition_variable condition;
            std::unique_lock ulock(wait_lock, std::defer_lock);

#ifdef USE_LINENOISE_CLI
            char *str;

            linenoiseHistorySetMaxLen(256);
            linenoiseSetCompletionCallback(LinenoiseCompletionCallback);

            while ((str = linenoise("sp> ")) != nullptr) {
                if (!str) continue;
                if (*str == '\0') {
                    linenoiseFree(str);
                    continue;
                }

                ulock.lock();
                AddHistory(str);
                QueueParseAndExecute(str, chrono_clock::now(), &condition);
                condition.wait(ulock);
                ulock.unlock();

                linenoiseHistoryAdd(str);
                linenoiseFree(str);
            }
#else
            std::string str;

            while (std::getline(std::cin, str)) {
                if (str.empty()) continue;

                ulock.lock();
                AddHistory(str);
                QueueParseAndExecute(str, chrono_clock::now(), &condition);
                condition.wait(ulock);
                ulock.unlock();
            }
#endif
        });
        cliInputThread.detach();
    }

    void ConsoleManager::AddLine(logging::Level lvl, const string &line) {
        std::lock_guard lock(linesLock);
        outputLines.push_back({lvl, line});
    }

    void ConsoleManager::StartThread(const ConsoleScript *startupScript) {
        if (startupScript) {
            exitOnEmptyQueue = true;
            for (const string &line : startupScript->Lines()) {
                scriptCommands.emplace(line);
            }
        }
        RegisteredThread::StartThread();
    }

    bool ConsoleManager::ThreadInit() {
        RegisterCoreCommands();
        RegisterTracyCommands();
        return true;
    }

    void ConsoleManager::Frame() {
        ZoneScoped;
        std::unique_lock<std::mutex> ulock(queueLock, std::defer_lock);

        ulock.lock();
        if (exitOnEmptyQueue && queuedCommands.empty() && scriptCommands.empty()) {
            ulock.unlock();
            ParseAndExecute("exit");
            StopThread(false);
            return;
        }

        auto now = chrono_clock::now();
        while (!scriptCommands.empty() && (queuedCommands.empty() || queuedCommands.top().wait_until > now)) {
            auto text = scriptCommands.front();
            scriptCommands.pop();
            ulock.unlock();

            ParseAndExecute(text);

            ulock.lock();
            now = chrono_clock::now();
        }
        while (!queuedCommands.empty()) {
            auto top = queuedCommands.top();
            if (top.wait_until > now) break;

            queuedCommands.pop();
            ulock.unlock();

            ParseAndExecute(top.text);
            if (top.handled != nullptr) {
                top.handled->notify_all();
            }

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
        Debugf("Executing console command: %s %s", cmd, args);
        std::lock_guard execLock(cvarExecLock);
        CVarBase *cvar = nullptr;
        {
            std::shared_lock readLock(cvarReadLock);
            auto cvarit = cvars.find(to_lower_copy(cmd));
            if (cvarit != cvars.end()) {
                cvar = cvarit->second;
            }
        }
        if (cvar) {
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
        if (history.size() == 0 || history[history.size() - 1] != input) {
            history.push_back(input);
        }
    }

    vector<string> ConsoleManager::AllHistory(size_t maxEntries) {
        maxEntries = std::min(maxEntries, history.size());
        if (maxEntries == 0) return {};

        std::lock_guard lock(historyLock);
        vector<string> results(history.rbegin(), history.rbegin() + maxEntries);
        return results;
    }

    ConsoleManager::Completions ConsoleManager::AllCompletions(const string &rawInput, bool requestNewCompletions) {
        std::shared_lock lock(cvarReadLock);

        Completions result;
        result.pending = false;

        auto input = to_lower_copy(rawInput);
        auto it = cvars.lower_bound(input);

        if (it != cvars.begin()) {
            it--;
            auto cvar = it->second;
            auto &name = cvar->GetName();
            if (input.size() > name.size() && input[name.size()] == ' ' && starts_with(input, cvar->GetNameLower())) {
                if (requestNewCompletions) cvar->RequestCompletion();
                if (cvar->PendingCompletion()) result.pending = true;

                auto restOfLine = string_view(input).substr(name.size() + 1);
                cvar->EachCompletion([&](const string &completion) {
                    if (starts_with(to_lower_copy(completion), restOfLine)) {
                        result.values.push_back(name + " " + completion);
                    }
                });
            }
            it++;
        }

        for (; it != cvars.end(); it++) {
            auto cvar = it->second;
            if (starts_with(cvar->GetNameLower(), input)) {
                result.values.push_back(cvar->GetName());
            } else {
                break;
            }
        }

        return result;
    }

#ifdef USE_LINENOISE_CLI
    void LinenoiseCompletionCallback(const char *buf, linenoiseCompletions *lc) {
        auto completions = GetConsoleManager().AllCompletions(string(buf), true);
        for (auto str : completions.values) {
            linenoiseAddCompletion(lc, str.c_str());
        }
    }
#endif
} // namespace sp
