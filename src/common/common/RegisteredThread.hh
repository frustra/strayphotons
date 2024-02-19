/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

#include <atomic>
#include <string>
#include <thread>

namespace sp {
    class RegisteredThread : public NonCopyable {
    public:
        RegisteredThread(std::string threadName, chrono_clock::duration interval, bool traceFrames = false);
        RegisteredThread(std::string threadName, double framesPerSecond, bool traceFrames = false);
        virtual ~RegisteredThread();

        void Pause(bool pause = true);
        void Step(unsigned int count = 1);
        std::thread::id GetThreadId() const;

        const std::string threadName;
        chrono_clock::duration interval;
        std::atomic_uint64_t stepCount, maxStepCount;
        std::atomic_bool stepMode;
        const bool traceFrames = false;

    protected:
        void StartThread(bool startPaused = false);
        void StopThread(bool waitForExit = true);

        // Will be called once per interval except for in step mode
        virtual void Frame() = 0;
        // Will always be called once per interval
        virtual void PreFrame(){};
        // Will always be called once per interval
        virtual void PostFrame(bool stepMode){};

        // Will be called once in the thread, before the first call to Frame()
        // If this returns false, the thread will be stopped
        virtual bool ThreadInit() {
            return true;
        }

        enum class ThreadState : uint32_t {
            Stopped = 0,
            Started,
            Stopping,
        };
        std::atomic<ThreadState> state;

    private:
        std::thread thread;
    };
} // namespace sp
